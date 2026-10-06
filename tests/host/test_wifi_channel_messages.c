#include "wifi_channel_messages.h"
#include "app_wifi_driver.h"
#include "app_console.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

struct fake_queue { unsigned capacity, count; size_t size; app_message_t data[4]; };
static TaskFunction_t tasks[2];
static void *contexts[2];
static jmp_buf escape;
static int running_task = -1;
static int64_t now = 1000;
static unsigned created, queue_calls, fail_queue_at, fail_task_at, opened, closed, returned, reply_count;
static unsigned generation = 7;
static bool cancel_in_io, stop_in_io, late_reply;
static app_message_t responses[64], close_request;
static const void *expected_buffer;
static app_wifi_result_t io_result;
static void run(unsigned i)
{
    assert(tasks[i]); running_task = (int)i;
    if (!setjmp(escape)) tasks[i](contexts[i]);
    running_task = -1;
}
QueueHandle_t xQueueCreate(unsigned capacity, size_t size)
{
    assert((capacity == 1 || capacity == 4) && size == sizeof(app_message_t));
    if (++queue_calls == fail_queue_at) return NULL;
    struct fake_queue *q = calloc(1, sizeof(*q)); q->capacity = capacity; q->size = size; return q;
}
void vQueueDelete(QueueHandle_t q) { assert(!q->count); free(q); }
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    assert(!wait); if (q->count == q->capacity) return pdFALSE;
    memcpy(&q->data[q->count++], item, q->size); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait)
{
    if (!q->count) {
        if (wait && !stop_in_io) longjmp(escape, 1);
        return pdFALSE;
    }
    memcpy(item, &q->data[0], q->size); --q->count;
    memmove(q->data, q->data + 1, q->count * q->size); return pdTRUE;
}
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack,
    void *context, unsigned priority, void *handle)
{
    assert(stack == 4096 && priority == 2 && !handle);
    unsigned lane = !strcmp(name, "wifi_tcp_command") ? 0 : 1;
    assert(lane == 0 || !strcmp(name, "wifi_tcp_event"));
    if (++created == fail_task_at) return pdFALSE;
    tasks[lane] = fn; contexts[lane] = context; return pdPASS;
}
void vTaskDelete(void *task) { assert(!task && running_task >= 0); tasks[running_task] = NULL; longjmp(escape, 1); }
void vTaskDelay(TickType_t ticks)
{
    now += (int64_t)ticks * 1000;
    bool saved = stop_in_io; stop_in_io = true;
    for (unsigned i = 0; i < 2; ++i) if (tasks[i]) run(i);
    stop_in_io = saved;
}
int64_t esp_timer_get_time(void) { return now; }
esp_err_t app_console_reply(const app_message_t *request, app_message_t *reply)
{
    reply->correlation_id = request->correlation_id;
    responses[reply_count++] = *reply;
    if (reply->lease && !late_reply) {
        size_t capacity = 0;
        assert(app_message_lease_data(reply->lease, &capacity) == expected_buffer && capacity == 8);
    }
    app_message_release(reply); return late_reply ? ESP_ERR_TIMEOUT : ESP_OK;
}
static void lease_return(void *context) { assert(context == (void *)expected_buffer); ++returned; }
static app_wifi_result_t start(void *ctx, const network_config_t *config) { (void)ctx; (void)config; return APP_WIFI_OK; }
static app_wifi_result_t stop(void *ctx, uint32_t timeout) { (void)ctx; (void)timeout; return APP_WIFI_OK; }
static app_wifi_result_t status(void *ctx, app_wifi_status_t *out)
{ (void)ctx; *out = (app_wifi_status_t){.online = true, .max_channel = 13, .generation = generation}; return APP_WIFI_OK; }
static app_wifi_result_t clients(void *ctx, app_wifi_client_t *out, size_t capacity, size_t *count)
{ (void)ctx; (void)out; (void)capacity; *count = 0; return APP_WIFI_OK; }
static void destroy(void *ctx) { (void)ctx; }
static app_wifi_result_t open_channel(void *ctx, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, void **channel)
{
    (void)ctx; assert(endpoint->port == 15740 && deadline->at_us == 1000000 && deadline->generation == generation);
    assert(!deadline->cancelled(deadline->context)); *channel = malloc(1); ++opened; return APP_WIFI_OK;
}
static app_wifi_result_t io(void *ctx, void *data, size_t size, bool transmit,
    const app_wifi_deadline_t *deadline, size_t *bytes)
{
    assert(ctx && data == expected_buffer && size == 8 && deadline->at_us == 1000000);
    (void)transmit;
    if (cancel_in_io) assert(wifi_channel_messages_dispatch(&close_request) == ESP_OK);
    if (stop_in_io) wifi_channel_messages_cancel();
    *bytes = deadline->cancelled(deadline->context) || io_result != APP_WIFI_OK ? 3 : size;
    return deadline->cancelled(deadline->context) ? APP_WIFI_CANCELLED : io_result;
}
static void close_channel(void *ctx) { free(ctx); ++closed; }
static app_wifi_result_t poll_channel(void *ctx, const app_wifi_deadline_t *deadline, bool *readable)
{ assert(ctx && deadline->generation == generation); *readable = true; return APP_WIFI_OK; }
static app_message_t request(app_message_type_t type, uint32_t token)
{
    static uint32_t correlation;
    app_message_t m = {.type = type, .source = APP_ENDPOINT_CAMERA, .target = APP_ENDPOINT_WIFI,
        .flags = APP_MESSAGE_REQUEST, .generation = 19, .deadline_us = 1000000, .correlation_id = ++correlation};
    m.payload.channel.token = token; m.payload.channel.generation = generation;
    m.payload.channel.length = 8; m.payload.channel.port = 15740;
    strcpy(m.payload.channel.address, "192.168.4.2"); return m;
}
static app_message_t bulk(app_message_type_t type, uint32_t token, bool writable)
{
    app_message_t m = request(type, token); m.flags |= APP_MESSAGE_BULK;
    assert(app_message_lease_create((void *)expected_buffer, 8, writable, lease_return,
        (void *)expected_buffer, &m.lease) == ESP_OK); return m;
}
static void shutdown(void)
{
    wifi_channel_messages_cancel(); stop_in_io = true;
    for (unsigned i = 0; i < 2; ++i) if (tasks[i]) run(i);
    assert(wifi_channel_messages_idle()); wifi_channel_messages_cleanup(); stop_in_io = false;
}
int main(void)
{
    const app_wifi_driver_ops_t ops = {.api_version = APP_WIFI_API_VERSION,
        .capabilities = APP_WIFI_CAP_AP | APP_WIFI_CAP_TCP, .start = start, .reconfigure = start,
        .stop = stop, .status = status, .clients = clients, .destroy = destroy,
        .channel_open = open_channel, .channel_io = io, .channel_close = close_channel, .channel_poll = poll_channel};
    int context; app_wifi_t *wifi = NULL; uint8_t data[8] = {0}; expected_buffer = data;
    assert(app_wifi_driver_bind(&ops, &context, &wifi) == APP_WIFI_OK);
    network_config_t config; network_config_make_default(&config); assert(app_wifi_start(wifi, &config) == APP_WIFI_OK);
    fail_queue_at = 2; assert(wifi_channel_messages_start(wifi) == ESP_ERR_NO_MEM); fail_queue_at = 0;
    fail_task_at = 1; assert(wifi_channel_messages_start(wifi) == ESP_ERR_NO_MEM); fail_task_at = 0;
    fail_task_at = created + 2; assert(wifi_channel_messages_start(wifi) == ESP_ERR_NO_MEM); fail_task_at = 0;
    assert(wifi_channel_messages_start(wifi) == ESP_OK);
    assert(wifi_channel_messages_start(wifi) == ESP_ERR_INVALID_STATE);
    app_message_t a = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0), b = a;
    assert(wifi_channel_messages_dispatch(&a) == ESP_OK); uint32_t token = a.payload.channel.token;
    assert(wifi_channel_messages_dispatch(&b) == ESP_OK); uint32_t other = b.payload.channel.token;
    app_message_t excess = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0);
    assert(wifi_channel_messages_dispatch(&excess) == ESP_ERR_NO_MEM);
    run(0); run(1); assert(opened == 2 && reply_count == 2);
    app_message_t m = bulk(APP_MESSAGE_WIFI_CHANNEL_SEND, token, false);
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK && !m.lease); run(0);
    assert(returned == 1 && responses[2].result == ESP_OK && responses[2].payload.channel.length == 8);
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, false);
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK); run(0);
    assert(returned == 2 && responses[3].payload.channel.status == APP_NETWORK_IO_INVALID);
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); m.source = APP_ENDPOINT_UART;
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_INVALID_STATE && m.lease); app_message_release(&m);
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); ++m.generation;
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_INVALID_STATE); app_message_release(&m);
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); --m.payload.channel.generation;
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_INVALID_STATE); app_message_release(&m);
    /* Queue pressure retains the rejected lease; dedicated CLOSE still enters
     * and cancels/reclaims before stale queued jobs can access the old channel. */
    for (unsigned i = 0; i < 4; ++i) { m = bulk(APP_MESSAGE_WIFI_CHANNEL_SEND, other, false); assert(wifi_channel_messages_dispatch(&m) == ESP_OK); }
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_SEND, other, false);
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_TIMEOUT && m.lease); app_message_release(&m);
    m = request(APP_MESSAGE_WIFI_CHANNEL_CLOSE, other); assert(wifi_channel_messages_dispatch(&m) == ESP_OK);
    run(1); assert(closed == 1 && returned == 10);
    assert(responses[8].payload.channel.status == APP_NETWORK_IO_OK);
    for (unsigned i = 4; i < 8; ++i) assert(responses[i].payload.channel.status == APP_NETWORK_IO_CANCELLED);
    close_request = request(APP_MESSAGE_WIFI_CHANNEL_CLOSE, token); cancel_in_io = true;
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); assert(wifi_channel_messages_dispatch(&m) == ESP_OK);
    run(0); cancel_in_io = false;
    assert(closed == 2 && returned == 11);
    assert(responses[9].payload.channel.status == APP_NETWORK_IO_CANCELLED && responses[9].payload.channel.length == 3);
    m = request(APP_MESSAGE_WIFI_CHANNEL_CLOSE, token);
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK && closed == 2);
    m = request(APP_MESSAGE_WIFI_CHANNEL_CLOSE, token); m.source = APP_ENDPOINT_UART;
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_INVALID_STATE);
    /* Late reply still returns exactly one reference. Network change rejects
     * the old channel without exposing an fd or a private driver pointer. */
    a = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0); assert(wifi_channel_messages_dispatch(&a) == ESP_OK); token = a.payload.channel.token; run(0);
    late_reply = true; ++generation;
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); --m.payload.channel.generation;
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK); run(0); late_reply = false;
    assert(responses[reply_count - 1].payload.channel.status == APP_NETWORK_IO_STALE && returned == 12);
    shutdown(); assert(closed == opened);
    assert(wifi_channel_messages_start(wifi) == ESP_OK);
    late_reply = true;
    a = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0); assert(wifi_channel_messages_dispatch(&a) == ESP_OK); run(0);
    assert(closed == opened); late_reply = false;
    a = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0); assert(wifi_channel_messages_dispatch(&a) == ESP_OK);
    m = request(APP_MESSAGE_WIFI_CHANNEL_CLOSE, 0); m.payload.channel.opening_correlation = a.correlation_id;
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK); run(0);
    assert(closed == opened); shutdown();
    assert(wifi_channel_messages_start(wifi) == ESP_OK);
    a = request(APP_MESSAGE_WIFI_CHANNEL_OPEN, 0); assert(wifi_channel_messages_dispatch(&a) == ESP_OK); token = a.payload.channel.token; run(0);
    m = request(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token); m.payload.channel.poll = true; m.payload.channel.length = 0;
    assert(wifi_channel_messages_dispatch(&m) == ESP_OK); run(0);
    assert(responses[reply_count - 1].payload.channel.readable && !responses[reply_count - 1].lease);
    m = request(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token); m.payload.channel.poll = true;
    assert(wifi_channel_messages_dispatch(&m) == ESP_ERR_INVALID_ARG);
    stop_in_io = true;
    m = bulk(APP_MESSAGE_WIFI_CHANNEL_RECEIVE, token, true); assert(wifi_channel_messages_dispatch(&m) == ESP_OK); run(0);
    assert(returned == 13 && responses[reply_count - 1].payload.channel.length == 3);
    run(1); assert(wifi_channel_messages_idle()); wifi_channel_messages_cleanup(); stop_in_io = false;
    assert(closed == opened);
    assert(app_wifi_stop(wifi, 100) == APP_WIFI_OK && app_wifi_destroy(&wifi) == APP_WIFI_OK);
    return 0;
}
