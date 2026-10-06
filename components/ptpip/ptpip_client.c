#include "ptpip_client.h"
#include "app_console.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static bool valid(const ptpip_client_t *client, ptpip_channel_kind_t kind)
{ return client && client->api_version == PTPIP_CLIENT_API_VERSION && (unsigned)kind < PTPIP_CHANNEL_COUNT; }
static bool stopped(ptpip_client_t *client)
{
    return atomic_load(&client->cancelled) ||
        (client->cancel_predicate && client->cancel_predicate(client->cancel_context));
}
static bool stale(ptpip_client_t *client, ptpip_channel_kind_t kind)
{
    uint32_t observed = atomic_load(&client->network_generation);
    return client->channels[kind].token && observed && observed != client->channels[kind].generation;
}
static int64_t request_deadline(ptpip_client_t *client, unsigned timeout_ms)
{
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    if (client->transaction_depth && client->transaction_deadline < deadline)
        deadline = client->transaction_deadline;
    return deadline;
}
static app_message_t envelope(ptpip_client_t *client, app_message_type_t type, int64_t deadline)
{
    return (app_message_t){.type = type, .source = client->endpoint, .target = APP_ENDPOINT_WIFI,
        .flags = APP_MESSAGE_REQUEST, .generation = client->generation, .deadline_us = deadline};
}
typedef struct { ptpip_client_t *client; ptpip_channel_kind_t kind; app_message_t *request; } wait_context_t;
static bool cancel_wait(void *context)
{
    wait_context_t *wait = context;
    if (!stopped(wait->client) && !stale(wait->client, wait->kind)) return false;
    /* OPEN has not supplied its opaque token yet. Its correlation identifies
     * the reservation; CLOSE carries values only, never a pointer/predicate. */
    app_message_t close = envelope(wait->client, APP_MESSAGE_WIFI_CHANNEL_CLOSE, esp_timer_get_time() + 1000000);
    close.correlation_id = wait->request->correlation_id;
    close.payload.channel.token = wait->request->payload.channel.token;
    close.payload.channel.generation = wait->request->payload.channel.generation;
    close.payload.channel.opening_correlation = wait->request->correlation_id;
    app_console_send(&close);
    return true;
}
static bool result(ptpip_client_t *client, esp_err_t error, const app_message_t *reply)
{
    client->diagnostic = error;
    if (error == ESP_OK) error = reply->result;
    client->diagnostic = error;
    if (error == ESP_OK && reply->payload.channel.status == APP_NETWORK_IO_OK) {
        client->last_result = PTPIP_CLIENT_OK; return true;
    }
    if (stopped(client) || reply->payload.channel.status == APP_NETWORK_IO_CANCELLED)
        client->last_result = PTPIP_CLIENT_CANCELLED;
    else if (error == ESP_ERR_TIMEOUT || reply->payload.channel.status == APP_NETWORK_IO_TIMEOUT)
        client->last_result = PTPIP_CLIENT_TIMEOUT;
    else client->last_result = PTPIP_CLIENT_NETWORK;
    return false;
}
bool ptpip_client_init(ptpip_client_t *client, app_endpoint_t endpoint, uint32_t generation,
    bool (*cancelled)(void *context), void *context)
{
    if (!client || endpoint <= APP_ENDPOINT_NONE || endpoint >= APP_ENDPOINT_COUNT ||
        endpoint == APP_ENDPOINT_WIFI || !generation) return false;
    *client = (ptpip_client_t){.api_version = PTPIP_CLIENT_API_VERSION, .endpoint = endpoint,
        .generation = generation, .next_transaction = 1, .cancel_predicate = cancelled, .cancel_context = context};
    atomic_init(&client->cancelled, false); atomic_init(&client->network_generation, 0);
    for (unsigned i = 0; i < PTPIP_CHANNEL_COUNT; ++i) client->channels[i].timeout_ms = 5000;
    return true;
}
void ptpip_client_cancel(ptpip_client_t *client) { if (client) atomic_store(&client->cancelled, true); }
void ptpip_client_network_changed(ptpip_client_t *client, uint32_t generation)
{ if (client && generation) atomic_store(&client->network_generation, generation); }
bool ptpip_client_open(ptpip_client_t *client, ptpip_channel_kind_t kind,
    const char *address, uint16_t port, unsigned timeout_ms)
{
    if (!valid(client, kind) || !address || strlen(address) >= 16 || !port || !timeout_ms || client->channels[kind].token) return false;
    if (stopped(client)) { client->last_result = PTPIP_CLIENT_CANCELLED; return false; }
    app_message_t request = envelope(client, APP_MESSAGE_WIFI_CHANNEL_OPEN, request_deadline(client, timeout_ms)), reply = {0};
    memcpy(request.payload.channel.address, address, strlen(address) + 1);
    request.payload.channel.port = port;
    request.payload.channel.generation = atomic_load(&client->network_generation);
    wait_context_t wait = {client, kind, &request};
    esp_err_t error = app_console_request_cancelable(&request, &reply, cancel_wait, &wait);
    bool ok = result(client, error, &reply);
    if (ok && (!reply.payload.channel.token || !reply.payload.channel.generation)) {
        client->last_result = PTPIP_CLIENT_PROTOCOL; ok = false;
    }
    if (ok) {
        client->channels[kind].token = reply.payload.channel.token;
        client->channels[kind].generation = reply.payload.channel.generation;
        unsigned expected = 0;
        atomic_compare_exchange_strong(&client->network_generation, &expected, reply.payload.channel.generation);
        if (atomic_load(&client->network_generation) != reply.payload.channel.generation) {
            ptpip_client_close(client, kind, 1000);
            client->last_result = PTPIP_CLIENT_NETWORK; ok = false;
        }
    }
    app_message_release(&reply); return ok;
}
bool ptpip_client_close(ptpip_client_t *client, ptpip_channel_kind_t kind, unsigned timeout_ms)
{
    if (!valid(client, kind) || !timeout_ms) return false;
    if (!client->channels[kind].token) return true;
    app_message_t request = envelope(client, APP_MESSAGE_WIFI_CHANNEL_CLOSE, request_deadline(client, timeout_ms)), reply = {0};
    request.payload.channel.token = client->channels[kind].token;
    request.payload.channel.generation = client->channels[kind].generation;
    bool ok = result(client, app_console_request(&request, &reply), &reply);
    if (ok) {
        client->channels[kind].token = client->channels[kind].generation = 0;
        if (kind == PTPIP_CHANNEL_COMMAND) {
            client->command_initialized = client->event_initialized = false;
            client->connection_id = client->session = 0;
            client->next_transaction = 1;
        } else client->event_initialized = false;
    }
    app_message_release(&reply); return ok;
}
static void lease_returned(void *context) { atomic_store((atomic_bool *)context, true); }
bool ptpip_client_poll(ptpip_client_t *client, ptpip_channel_kind_t kind, bool *readable)
{
    if (readable) *readable = false;
    if (!valid(client, kind) || !readable || !client->channels[kind].token) return false;
    if (stopped(client)) { client->last_result = PTPIP_CLIENT_CANCELLED; return false; }
    if (stale(client, kind)) { client->last_result = PTPIP_CLIENT_NETWORK; return false; }
    int64_t deadline = esp_timer_get_time() + (int64_t)client->channels[kind].timeout_ms * 1000;
    if (client->transaction_depth && client->transaction_deadline < deadline) deadline = client->transaction_deadline;
    app_message_t request = envelope(client, APP_MESSAGE_WIFI_CHANNEL_RECEIVE, deadline), reply = {0};
    request.payload.channel.token = client->channels[kind].token;
    request.payload.channel.generation = client->channels[kind].generation;
    request.payload.channel.poll = true;
    wait_context_t wait = {client, kind, &request};
    esp_err_t error = app_console_request_cancelable(&request, &reply, cancel_wait, &wait);
    bool ok = result(client, error, &reply);
    if (ok) *readable = reply.payload.channel.readable;
    app_message_release(&reply); return ok;
}
bool ptpip_client_transfer(ptpip_client_t *client, ptpip_channel_kind_t kind,
    void *buffer, size_t size, bool transmit)
{
    if (!valid(client, kind) || (!buffer && size) || !client->channels[kind].token) return false;
    client->transferred = 0;
    if (stopped(client)) { client->last_result = PTPIP_CLIENT_CANCELLED; return false; }
    if (stale(client, kind)) { client->last_result = PTPIP_CLIENT_NETWORK; return false; }
    if (!size) { client->last_result = PTPIP_CLIENT_OK; return true; }
    int64_t deadline = esp_timer_get_time() + (int64_t)client->channels[kind].timeout_ms * 1000;
    if (client->transaction_depth && client->transaction_deadline < deadline) deadline = client->transaction_deadline;
    app_message_t request = envelope(client, transmit ? APP_MESSAGE_WIFI_CHANNEL_SEND : APP_MESSAGE_WIFI_CHANNEL_RECEIVE, deadline), reply = {0};
    request.flags |= APP_MESSAGE_BULK;
    request.payload.channel.token = client->channels[kind].token;
    request.payload.channel.generation = client->channels[kind].generation;
    request.payload.channel.length = size;
    atomic_bool returned; atomic_init(&returned, false);
    esp_err_t error = app_message_lease_create(buffer, size, !transmit, lease_returned, &returned, &request.lease);
    if (error != ESP_OK) { client->diagnostic = error; client->last_result = PTPIP_CLIENT_NETWORK; return false; }
    wait_context_t wait = {client, kind, &request};
    error = app_console_request_cancelable(&request, &reply, cancel_wait, &wait);
    client->transferred = reply.payload.channel.length;
    bool ok = result(client, error, &reply);
    if (ok && client->transferred != size) { client->last_result = PTPIP_CLIENT_PROTOCOL; ok = false; }
    app_message_release(&reply);
    while (!atomic_load(&returned)) vTaskDelay(1);
    return ok;
}
bool ptpip_client_timeout_set(ptpip_client_t *client, ptpip_channel_kind_t kind, unsigned timeout_ms)
{
    if (!valid(client, kind) || !timeout_ms) return false;
    client->channels[kind].timeout_ms = timeout_ms; return true;
}
bool ptpip_client_transaction_begin(ptpip_client_t *client, ptpip_channel_kind_t kind)
{
    if (!valid(client, kind) || !client->channels[kind].token || client->transaction_depth >= 8) return false;
    if (stopped(client)) { client->last_result = PTPIP_CLIENT_CANCELLED; return false; }
    if (stale(client, kind)) { client->last_result = PTPIP_CLIENT_NETWORK; return false; }
    int64_t deadline = esp_timer_get_time() + (int64_t)client->channels[kind].timeout_ms * 1000;
    if (client->transaction_depth && client->transaction_deadline < deadline) deadline = client->transaction_deadline;
    if (deadline <= esp_timer_get_time()) {
        client->last_result = PTPIP_CLIENT_TIMEOUT; return false;
    }
    client->transaction_parents[client->transaction_depth] = client->transaction_deadline;
    client->transaction_deadline = deadline;
    ++client->transaction_depth; return true;
}
bool ptpip_client_scope_begin(ptpip_client_t *client, unsigned timeout_ms)
{
    if (!valid(client, PTPIP_CHANNEL_COMMAND) || !timeout_ms || client->transaction_depth) return false;
    if (stopped(client)) { client->last_result = PTPIP_CLIENT_CANCELLED; return false; }
    return ptpip_client_cleanup_begin(client, timeout_ms);
}
bool ptpip_client_cleanup_begin(ptpip_client_t *client, unsigned timeout_ms)
{
    if (!valid(client, PTPIP_CHANNEL_COMMAND) || !timeout_ms || client->transaction_depth) return false;
    client->transaction_depth = 1;
    client->transaction_deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    return true;
}
void ptpip_client_transaction_end(ptpip_client_t *client)
{
    if (client && client->transaction_depth) {
        --client->transaction_depth;
        client->transaction_deadline = client->transaction_depth ?
            client->transaction_parents[client->transaction_depth] : 0;
    }
}
