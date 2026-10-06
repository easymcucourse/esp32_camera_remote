#include "ptpip_client.h"
#include "app_console.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>
static int64_t now = 1000;
static unsigned requests, closes, lease_delay;
static uint32_t next_token = 41;
static bool stop, cancel_during_wait, late, fail_close, short_reply, generation_during_open;
static app_message_t held;
static app_network_io_status_t response_status;
static ptpip_client_t *active;
static void *expected_buffer;
static int64_t expected_deadline;
int64_t esp_timer_get_time(void) { return now; }
void vTaskDelay(TickType_t ticks)
{
    now += (int64_t)ticks * 1000;
    if (held.lease && ++lease_delay == 3) app_message_release(&held);
}
static bool cancelled(void *context) { assert(context == &stop); return stop; }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->type == APP_MESSAGE_WIFI_CHANNEL_CLOSE && message->source == APP_ENDPOINT_CAMERA);
    assert(message->correlation_id == 19 && message->payload.channel.opening_correlation == 19);
    ++closes; return ESP_OK;
}
static esp_err_t exchange(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn predicate, void *context)
{
    assert(request->source == APP_ENDPOINT_CAMERA && request->target == APP_ENDPOINT_WIFI && request->generation == active->generation);
    ++requests; request->correlation_id = 19;
    if (expected_deadline) assert(request->deadline_us == expected_deadline);
    if (cancel_during_wait) {
        stop = true; assert(predicate && predicate(context));
        held = *request; request->lease = NULL;
        return ESP_ERR_INVALID_STATE;
    }
    if (predicate) assert(!predicate(context));
    reply->payload.channel = request->payload.channel;
    reply->payload.channel.status = response_status;
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_OPEN) {
        reply->payload.channel.token = ++next_token; reply->payload.channel.generation = 7;
        if (generation_during_open) ptpip_client_network_changed(active, 8);
    } else if (request->type == APP_MESSAGE_WIFI_CHANNEL_CLOSE) {
        ++closes; if (fail_close) return ESP_ERR_TIMEOUT;
    } else {
        size_t size = 0;
        assert(app_message_lease_data(request->lease, &size) == expected_buffer && size == 8);
        assert((app_message_lease_write(request->lease, NULL) != NULL) == (request->type == APP_MESSAGE_WIFI_CHANNEL_RECEIVE));
        if (late) { held = *request; request->lease = NULL; return ESP_ERR_TIMEOUT; }
        reply->lease = request->lease; request->lease = NULL;
        if (short_reply || response_status != APP_NETWORK_IO_OK) reply->payload.channel.length = 3;
    }
    return ESP_OK;
}
esp_err_t app_console_request(app_message_t *request, app_message_t *reply) { return exchange(request, reply, NULL, NULL); }
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn predicate, void *context) { return exchange(request, reply, predicate, context); }
int main(void)
{
    ptpip_client_t client, second; active = &client;
    assert(!ptpip_client_init(&client, APP_ENDPOINT_WIFI, 9, cancelled, &stop));
    assert(ptpip_client_init(&client, APP_ENDPOINT_CAMERA, 9, cancelled, &stop));
    assert(ptpip_client_init(&second, APP_ENDPOINT_CAMERA, 9, NULL, NULL));
    assert(ptpip_client_open(&client, PTPIP_CHANNEL_COMMAND, "192.168.4.2", 15740, 800));
    assert(client.channels[0].token == 42 && client.channels[0].generation == 7);
    assert(!ptpip_client_open(&client, PTPIP_CHANNEL_COMMAND, "192.168.4.2", 15740, 800));
    assert(ptpip_client_open(&client, PTPIP_CHANNEL_EVENT, "192.168.4.2", 15740, 5000));
    uint8_t data[8] = {0}; expected_buffer = data;
    expected_deadline = now + 5000000;
    assert(ptpip_client_transaction_begin(&client, PTPIP_CHANNEL_COMMAND));
    now += 100000;
    assert(ptpip_client_transaction_begin(&client, PTPIP_CHANNEL_EVENT));
    assert(client.transaction_deadline == expected_deadline && client.transaction_depth == 2 && !second.transaction_depth);
    assert(ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), true));
    assert(ptpip_client_transfer(&client, PTPIP_CHANNEL_EVENT, data, sizeof(data), false));
    response_status = APP_NETWORK_IO_TIMEOUT;
    assert(!ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), false));
    assert(client.last_result == PTPIP_CLIENT_TIMEOUT && client.transferred == 3);
    response_status = APP_NETWORK_IO_OK; short_reply = true;
    assert(!ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), true) && client.last_result == PTPIP_CLIENT_PROTOCOL);
    short_reply = false; late = true;
    int64_t before = now;
    assert(!ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), false));
    assert(!held.lease && now - before == 3000 && client.last_result == PTPIP_CLIENT_TIMEOUT);
    late = false; lease_delay = 0; cancel_during_wait = true;
    assert(!ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), false));
    assert(client.last_result == PTPIP_CLIENT_CANCELLED && !held.lease && closes == 1);
    cancel_during_wait = stop = false;
    ptpip_client_transaction_end(&client); ptpip_client_transaction_end(&client);
    assert(!client.transaction_depth && !client.transaction_deadline); expected_deadline = 0;
    ptpip_client_network_changed(&client, 8); unsigned count = requests;
    assert(!ptpip_client_transfer(&client, PTPIP_CHANNEL_COMMAND, data, sizeof(data), false) && requests == count);
    client.command_initialized = client.event_initialized = true;
    client.connection_id = 55; client.session = 1; client.next_transaction = 30;
    fail_close = true; assert(!ptpip_client_close(&client, PTPIP_CHANNEL_COMMAND, 1000) && client.channels[0].token);
    assert(client.command_initialized && client.event_initialized && client.session == 1 && client.connection_id == 55);
    fail_close = false; assert(ptpip_client_close(&client, PTPIP_CHANNEL_COMMAND, 1000) && !client.channels[0].token);
    assert(!client.command_initialized && !client.event_initialized && !client.session && !client.connection_id && client.next_transaction == 1);
    assert(ptpip_client_close(&client, PTPIP_CHANNEL_EVENT, 1000));
    ptpip_client_cancel(&client); assert(!ptpip_client_transaction_begin(&client, PTPIP_CHANNEL_COMMAND));
    assert(ptpip_client_init(&client, APP_ENDPOINT_CAMERA, 9, cancelled, &stop));
    generation_during_open = true;
    assert(!ptpip_client_open(&client, PTPIP_CHANNEL_COMMAND, "192.168.4.2", 15740, 800));
    assert(client.last_result == PTPIP_CLIENT_NETWORK && atomic_load(&client.network_generation) == 8 && !client.channels[0].token);
    generation_during_open = false;
    assert(ptpip_client_init(&client, APP_ENDPOINT_CAMERA, 10, cancelled, &stop));
    assert(ptpip_client_scope_begin(&client, 20000));
    int64_t outer = client.transaction_deadline;
    assert(!ptpip_client_scope_begin(&client, 1000));
    /* OPEN/CLOSE share the outer deadline without requiring a live token. */
    expected_deadline = now + 800000;
    assert(ptpip_client_open(&client, PTPIP_CHANNEL_COMMAND, "192.168.4.2", 15740, 800));
    assert(client.transaction_deadline == outer);
    assert(ptpip_client_timeout_set(&client, PTPIP_CHANNEL_COMMAND, 5000));
    assert(ptpip_client_transaction_begin(&client, PTPIP_CHANNEL_COMMAND));
    assert(client.transaction_deadline < outer);
    ptpip_client_transaction_end(&client); assert(client.transaction_deadline == outer);
    now = outer;
    assert(!ptpip_client_transaction_begin(&client, PTPIP_CHANNEL_COMMAND) && client.last_result == PTPIP_CLIENT_TIMEOUT);
    assert(client.transaction_depth == 1);
    ptpip_client_transaction_end(&client); assert(!client.transaction_deadline);
    ptpip_client_cancel(&client); assert(!ptpip_client_scope_begin(&client, 1000));
    assert(ptpip_client_cleanup_begin(&client, 1000));
    expected_deadline = now + 1000000;
    assert(ptpip_client_close(&client, PTPIP_CHANNEL_COMMAND, 5000));
    ptpip_client_transaction_end(&client);
    return 0;
}
