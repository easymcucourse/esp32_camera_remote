#include "app_console.h"
#include "app_message_internal.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include "fake_router_runtime.h"

static unsigned returned;
static unsigned wait_mode;
static app_message_t held_request;
static const app_endpoint_config_t config = {2, 1};
static char data[16] = "jpeg-payload";
static unsigned cancel_checks;
static bool cancel_waiter(void *context)
{
    assert(context == &cancel_checks);
    app_console_status_t current; app_console_get_status(&current);
    assert(current.pending == 1); /* callback outside mutex */
    return ++cancel_checks == 2;
}

static void give_back(void *context)
{
    assert(context == data); ++returned;
    app_console_status_t status; app_console_get_status(&status);
}
static app_message_t message(app_message_type_t type, app_endpoint_t source, app_endpoint_t target)
{
    return (app_message_t){.type = type, .source = source, .target = target,
        .generation = 22, .deadline_us = now_us + 100000};
}
static void attach(app_message_t *m, bool writable)
{ assert(app_message_lease_create(data, sizeof(data), writable, give_back, data, &m->lease) == ESP_OK); }

static void wifi_reply(void)
{
    app_message_t request, reply = {0};
    assert(app_console_receive(APP_ENDPOINT_WIFI, &request, 0) == ESP_OK);
    reply.payload.command.value = 81;
    assert(app_console_reply(&request, &reply) == ESP_OK);
    app_message_release(&request);
}
static void batch_reply(void)
{
    app_console_status_t current;app_console_get_status(&current);assert(current.pending==2);
    app_message_t camera,wifi,reply={0};
    assert(app_console_receive(APP_ENDPOINT_CAMERA,&camera,0)==ESP_OK);
    assert(app_console_receive(APP_ENDPOINT_WIFI,&wifi,0)==ESP_OK);
    assert(camera.correlation_id!=wifi.correlation_id);
    reply.payload.command.value=81;if(wait_mode==7)mutex_busy_ms=3;
    assert(app_console_reply(&wifi,&reply)==ESP_OK);
    app_message_release(&wifi);
    if (wait_mode==1) { held_request=camera;return; }
    reply=(app_message_t){.payload.command={.value=73}};attach(&reply,false);
    if(wait_mode==7)mutex_busy_ms=3;
    assert(app_console_reply(&camera,&reply)==ESP_OK);app_message_release(&camera);
}
static void retire_before_enqueue(void)
{
    app_console_endpoint_stop(APP_ENDPOINT_CAMERA);
    assert(app_console_endpoint_register(APP_ENDPOINT_CAMERA,&config)==ESP_OK);
}
static void contend_before_enqueue(void) { mutex_busy_ms=3; }

static void camera_reply(void)
{
    app_message_t request, reply = {0};
    assert(app_console_receive(APP_ENDPOINT_CAMERA, &request, 0) == ESP_OK);
    if (wait_mode == 1) { held_request = request; return; }
    if (wait_mode == 2) {
        app_message_t wrong = request;
        wrong.lease = NULL; wrong.source = request.target; wrong.target = request.source;
        wrong.flags = APP_MESSAGE_REPLY; ++wrong.generation;
        attach(&wrong, false);
        assert(app_console_send(&wrong) == ESP_ERR_INVALID_STATE && !wrong.lease);
        wrong.generation = request.generation; wrong.type = APP_MESSAGE_WIFI_STATUS;
        attach(&wrong, false);
        assert(app_console_send(&wrong) == ESP_ERR_INVALID_STATE && !wrong.lease);
    }
    if (wait_mode == 3) {
        app_console_endpoint_stop(APP_ENDPOINT_CAMERA);
        assert(app_console_endpoint_register(APP_ENDPOINT_CAMERA, &config) == ESP_OK);
        attach(&reply, false);
        assert(app_console_reply(&request, &reply) == ESP_ERR_INVALID_STATE && !reply.lease);
        app_message_release(&request); return;
    }
    if (wait_mode == 4) {
        now_us = request.deadline_us;
        app_console_router_poll();
        attach(&reply, false);
        assert(app_console_reply(&request, &reply) == ESP_ERR_TIMEOUT);
        app_message_release(&request); return;
    }
    if (wait_mode == 5) {
        app_message_t nested = message(APP_MESSAGE_WIFI_RSSI, APP_ENDPOINT_CAMERA, APP_ENDPOINT_WIFI), response;
        on_wait = wifi_reply;
        assert(app_console_request(&nested, &response) == ESP_OK);
        assert(response.payload.command.value == 81);
        app_message_release(&response);
    }
    if (wait_mode == 6) {
        request.payload.command.value = 73;
        assert(app_console_reply(&request, &request) == ESP_OK && !request.lease);
        return;
    }
    reply.payload.command.value = 73;
    attach(&reply, false);
    if (wait_mode==7) mutex_busy_ms=3;
    assert(app_console_reply(&request, &reply) == ESP_OK && !reply.lease);
    app_message_release(&request);
}

int main(void)
{
    app_message_t m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA), got;
    attach(&m, false);
    assert(app_console_send(&m) == ESP_ERR_INVALID_STATE && !m.lease && returned == 1);
    assert(app_console_router_start() == ESP_OK && tasks == 1);
    assert(app_console_router_start() == ESP_ERR_INVALID_STATE);
    m = message((app_message_type_t)-1, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    attach(&m, false); assert(app_console_send(&m) == ESP_ERR_INVALID_ARG && !m.lease);
    /* Pool exhaustion is bounded and the caller keeps rejected ownership. */
    app_message_t pooled[APP_MESSAGE_LEASE_CAPACITY] = {0};
    for (unsigned i = 0; i < APP_MESSAGE_LEASE_CAPACITY; ++i) attach(&pooled[i], false);
    app_message_lease_t *overflow = NULL;
    assert(app_message_lease_create(data, sizeof(data), false, give_back, data, &overflow) == ESP_ERR_NO_MEM && !overflow);
    for (unsigned i = 0; i < APP_MESSAGE_LEASE_CAPACITY; ++i) app_message_release(&pooled[i]);
    app_endpoint_t registered[] = {APP_ENDPOINT_CAMERA, APP_ENDPOINT_INPUT, APP_ENDPOINT_UI, APP_ENDPOINT_UART, APP_ENDPOINT_WIFI};
    for (unsigned i = 0; i < sizeof(registered) / sizeof(registered[0]); ++i)
        assert(app_console_endpoint_register(registered[i], &config) == ESP_OK);
    assert(app_console_subscribe(APP_MESSAGE_CAMERA_FRAME, APP_ENDPOINT_UI) == ESP_OK);
    assert(app_console_subscribe(APP_MESSAGE_CAMERA_FRAME, APP_ENDPOINT_UART) == ESP_OK);
    app_console_freeze_subscriptions();
    assert(app_console_subscribe(APP_MESSAGE_CAMERA_STATE, APP_ENDPOINT_UI) == ESP_ERR_INVALID_STATE);

    unsigned before = returned;
    m = message(APP_MESSAGE_CAMERA_FRAME, APP_ENDPOINT_CAMERA, APP_ENDPOINT_NONE);
    m.flags = APP_MESSAGE_EVENT | APP_MESSAGE_BULK; attach(&m, false);
    assert(app_console_send(&m) == ESP_OK && !m.lease);
    assert(app_console_receive(APP_ENDPOINT_UART, &got, 0) == ESP_OK);
    size_t size = 0; assert(app_message_lease_data(got.lease, &size) == data && size == sizeof(data));
    assert(!app_message_lease_write(got.lease, NULL));
    app_message_release(&got); app_message_release(&got); assert(returned == before);
    /* One subscriber is full, but the other receives a reference without a copy. */
    m = message(APP_MESSAGE_CAMERA_FRAME, APP_ENDPOINT_CAMERA, APP_ENDPOINT_NONE);
    m.flags = APP_MESSAGE_EVENT | APP_MESSAGE_BULK; attach(&m, false);
    assert(app_console_send(&m) == ESP_ERR_TIMEOUT && !m.lease);
    app_message_t stop = message(APP_MESSAGE_UI_MENU_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_UI);
    stop.payload.action.type = PAD_ACTION_RELEASE_ALL;
    assert(app_console_send(&stop) == ESP_OK);
    assert(app_console_receive(APP_ENDPOINT_UI, &got, 0) == ESP_OK && got.type == APP_MESSAGE_UI_MENU_ACTION);
    assert(app_console_receive(APP_ENDPOINT_UI, &got, 0) == ESP_OK && got.type == APP_MESSAGE_CAMERA_FRAME);
    app_message_release(&got); assert(returned == before + 1);
    assert(app_console_receive(APP_ENDPOINT_UART, &got, 0) == ESP_OK);
    app_message_release(&got); assert(returned == before + 2);

    /* All failure paths consume the producer lease. */
    m = message(APP_MESSAGE_CAMERA_FRAME, APP_ENDPOINT_CAMERA, APP_ENDPOINT_NONE);
    m.flags = APP_MESSAGE_EVENT | APP_MESSAGE_BULK; m.deadline_us = now_us - 1; attach(&m, false);
    assert(app_console_send(&m) == ESP_ERR_TIMEOUT && !m.lease);
    m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    m.generation = 0; attach(&m, false);
    assert(app_console_send(&m) == ESP_ERR_INVALID_ARG && !m.lease);
    for (unsigned i = 0; i < config.control_depth; ++i) {
        m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
        assert(app_console_send(&m) == ESP_OK);
    }
    attach(&m, false); assert(app_console_send(&m) == ESP_ERR_TIMEOUT && !m.lease);
    while (app_console_receive(APP_ENDPOINT_CAMERA, &got, 0) == ESP_OK) app_message_release(&got);

    m = message(APP_MESSAGE_CAMERA_STATUS, APP_ENDPOINT_UART, APP_ENDPOINT_CAMERA);
    m.deadline_us = now_us - 1; attach(&m, false);
    assert(app_console_request(&m, &got) == ESP_ERR_TIMEOUT && !m.lease);
    m = message(APP_MESSAGE_CAMERA_STATUS, APP_ENDPOINT_UART, APP_ENDPOINT_CAMERA);
    attach(&m, false); assert(app_console_request(&m, &m) == ESP_ERR_INVALID_ARG && !m.lease);
    for (wait_mode = 0; wait_mode <= 6; ++wait_mode) {
        app_message_t request = message(APP_MESSAGE_CAMERA_STATUS, APP_ENDPOINT_UART, APP_ENDPOINT_CAMERA), reply;
        attach(&request, true); on_wait = camera_reply;
        esp_err_t expected = wait_mode == 1 || wait_mode == 4 ? ESP_ERR_TIMEOUT :
                             wait_mode == 3 ? ESP_ERR_INVALID_STATE : ESP_OK;
        assert(app_console_request(&request, &reply) == expected && !request.lease);
        if (expected == ESP_OK) {
            assert(reply.payload.command.value == 73 && app_message_lease_data(reply.lease, NULL) == data);
            app_message_release(&reply);
        }
        if (wait_mode == 1) {
            app_message_t late = {0}; attach(&late, false);
            assert(app_console_reply(&held_request, &late) == ESP_ERR_TIMEOUT && !late.lease);
            app_message_release(&held_request);
        }
    }

    /* Request enqueue and its reply both survive brief router mutex
     * contention. The absolute deadline also bounds an unavailable mutex. */
    hold_housekeeping=true;wait_mode=7;on_wait=camera_reply;
    m=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    int64_t contention_started=now_us;on_unlock=contend_before_enqueue;
    assert(app_console_request(&m,&got)==ESP_OK);
    assert(now_us-contention_started==6000 && got.payload.command.value==73);
    app_message_release(&got);
    m=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    m.deadline_us=now_us+2000;on_unlock=contend_before_enqueue;
    contention_started=now_us;
    assert(app_console_request(&m,&got)==ESP_ERR_TIMEOUT);
    assert(now_us-contention_started==2000);
    assert(app_console_receive(APP_ENDPOINT_CAMERA,&got,0)==ESP_ERR_TIMEOUT);
    hold_housekeeping=false;

    /* Cancellation leaves queued consumer ownership intact; a late reply is
     * rejected and releases its own reference without revoking the buffer. */
    m = message(APP_MESSAGE_CAMERA_STATUS, APP_ENDPOINT_UART, APP_ENDPOINT_CAMERA);
    attach(&m, true); before = returned;
    int64_t cancel_start = now_us;
    assert(app_console_request_cancelable(&m, &got, cancel_waiter, &cancel_checks) == ESP_ERR_INVALID_STATE);
    assert(now_us - cancel_start <= 25000 && cancel_checks == 2 && returned == before);
    assert(app_console_receive(APP_ENDPOINT_CAMERA, &held_request, 0) == ESP_OK);
    app_message_t rejected = {0};
    rejected.lease = held_request.lease; held_request.lease = NULL;
    assert(app_console_reply(&held_request, &rejected) == ESP_ERR_INVALID_STATE);
    assert(returned == before + 1);

    /* Housekeeping returns queued expired leases even without an endpoint reader. */
    m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    attach(&m, false); assert(app_console_send(&m) == ESP_OK);
    before = returned; now_us += 101000; app_console_router_poll(); assert(returned == before + 1);
    assert(app_console_receive(APP_ENDPOINT_CAMERA, &got, 0) == ESP_ERR_TIMEOUT);
    uint32_t epoch = app_console_endpoint_generation(APP_ENDPOINT_CAMERA);
    m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    attach(&m, false); assert(app_console_send(&m) == ESP_OK);
    before = returned; app_console_endpoint_stop(APP_ENDPOINT_CAMERA); assert(returned == before + 1);
    assert(app_console_endpoint_register(APP_ENDPOINT_CAMERA, &config) == ESP_OK);
    assert(app_console_endpoint_generation(APP_ENDPOINT_CAMERA) != epoch);

    m=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    attach(&m,false);before=returned;on_unlock=retire_before_enqueue;
    assert(app_console_request(&m,&got)==ESP_ERR_INVALID_STATE && returned==before+1);
    assert(app_console_receive(APP_ENDPOINT_CAMERA,&got,0)==ESP_ERR_TIMEOUT);
    app_message_t batch[2],batch_results[2];esp_err_t transport[2];
    hold_housekeeping=true;wait_mode=7;on_wait=batch_reply;on_unlock=contend_before_enqueue;
    batch[0]=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    batch[1]=message(APP_MESSAGE_WIFI_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_WIFI);
    contention_started=now_us;
    assert(app_console_request_many(batch,batch_results,transport,2)==ESP_OK);
    assert(transport[0]==ESP_OK && transport[1]==ESP_OK && now_us-contention_started==9000);
    app_message_release(&batch_results[0]);app_message_release(&batch_results[1]);
    m=message(APP_MESSAGE_CAMERA_ACTION,APP_ENDPOINT_INPUT,APP_ENDPOINT_CAMERA);
    attach(&m,false);before=returned;mutex_busy_ms=3;contention_started=now_us;
    assert(app_console_send(&m)==ESP_ERR_TIMEOUT && returned==before+1 && now_us==contention_started);
    hold_housekeeping=false;
    batch[0]=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    batch[1]=message(APP_MESSAGE_WIFI_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_WIFI);
    wait_mode=0;on_wait=batch_reply;before=returned;
    assert(app_console_request_many(batch,batch_results,transport,2)==ESP_OK);
    assert(transport[0]==ESP_OK && transport[1]==ESP_OK && batch_results[0].payload.command.value==73 && batch_results[1].payload.command.value==81);
    assert(returned==before);app_message_release(&batch_results[0]);app_message_release(&batch_results[1]);assert(returned==before+1);
    batch[0]=message(APP_MESSAGE_CAMERA_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_CAMERA);
    batch[1]=message(APP_MESSAGE_WIFI_STATUS,APP_ENDPOINT_UART,APP_ENDPOINT_WIFI);
    wait_mode=1;on_wait=batch_reply;int64_t batch_started=now_us;
    assert(app_console_request_many(batch,batch_results,transport,2)==ESP_OK);
    assert(transport[0]==ESP_ERR_TIMEOUT && transport[1]==ESP_OK && now_us-batch_started==100000);
    app_message_release(&held_request);app_message_release(&batch_results[1]);
    assert(app_console_request_many(batch,batch_results,transport,0)==ESP_ERR_INVALID_ARG);

    /* Delivered leases cannot be revoked while a consumer is using the data. */
    m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    attach(&m, true); assert(app_console_send(&m) == ESP_OK);
    assert(app_console_receive(APP_ENDPOINT_CAMERA, &got, 0) == ESP_OK);
    assert(app_message_lease_write(got.lease, NULL) == data);
    assert(!app_console_router_quiesce(2));
    m = message(APP_MESSAGE_CAMERA_ACTION, APP_ENDPOINT_INPUT, APP_ENDPOINT_CAMERA);
    attach(&m, false); assert(app_console_send(&m) == ESP_ERR_INVALID_STATE && !m.lease);
    app_message_release(&got); assert(app_console_router_quiesce(2));
    app_console_status_t status; app_console_get_status(&status);
    assert(!status.running && !status.pending && !status.leases && !status.endpoints);
    assert(status.rejected && status.expired && status.late_replies);
    assert(app_console_router_start() == ESP_OK && tasks == 2);
    assert(app_console_endpoint_register(APP_ENDPOINT_CAMERA, &config) == ESP_OK);
    assert(app_console_router_quiesce(0));
    assert(task_exits==2);
    assert(app_console_router_start()==ESP_OK);
    hold_housekeeping=true;
    assert(!app_console_router_quiesce(0));
    assert(app_console_router_start()==ESP_ERR_INVALID_STATE);
    app_console_get_status(&status);assert(!status.running && !status.accepting && !status.leases && !status.pending);
    hold_housekeeping=false;run_housekeeping();
    assert(task_exits==3 && app_console_router_quiesce(0));
    puts("router priorities, request correlation, deadlines, epochs and lease ownership passed");
    return 0;
}
