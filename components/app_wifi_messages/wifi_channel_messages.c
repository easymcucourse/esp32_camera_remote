#include "private/wifi_channel_messages.h"
#include "app_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <string.h>

/* Two independent lanes for a camera's command and event streams. The endpoint
 * never waits for socket I/O. Each lane has one owner task and a separate close
 * inbox so a full bulk queue cannot prevent cancellation. */
#define CHANNEL_COUNT 2
typedef struct {
    QueueHandle_t jobs, control;
    app_wifi_channel_t *channel;
    app_endpoint_t owner;
    uint32_t token, owner_generation, network_generation, opening_correlation;
    bool reserved;
    atomic_bool closing;
} lane_t;
static lane_t lanes[CHANNEL_COUNT];
static app_wifi_t *network;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool stopping;
static atomic_uint workers;
static uint32_t next_token;
typedef struct { uint32_t token, generation; app_endpoint_t owner; } closed_token_t;
static closed_token_t closed_tokens[8];
static unsigned closed_index;

static app_network_io_status_t io_status(app_wifi_result_t result)
{
    switch (result) {
    case APP_WIFI_OK: return APP_NETWORK_IO_OK;
    case APP_WIFI_TIMEOUT: return APP_NETWORK_IO_TIMEOUT;
    case APP_WIFI_CANCELLED: return APP_NETWORK_IO_CANCELLED;
    case APP_WIFI_STALE: return APP_NETWORK_IO_STALE;
    case APP_WIFI_CLOSED: return APP_NETWORK_IO_CLOSED;
    case APP_WIFI_INVALID: return APP_NETWORK_IO_INVALID;
    case APP_WIFI_NO_MEMORY: return APP_NETWORK_IO_NO_MEMORY;
    default: return APP_NETWORK_IO_FAILED;
    }
}
static esp_err_t error_code(app_network_io_status_t result)
{
    switch (result) {
    case APP_NETWORK_IO_OK: return ESP_OK;
    case APP_NETWORK_IO_TIMEOUT: return ESP_ERR_TIMEOUT;
    case APP_NETWORK_IO_CANCELLED: case APP_NETWORK_IO_STALE: return ESP_ERR_INVALID_STATE;
    case APP_NETWORK_IO_INVALID: return ESP_ERR_INVALID_ARG;
    case APP_NETWORK_IO_NO_MEMORY: return ESP_ERR_NO_MEM;
    default: return ESP_FAIL;
    }
}
static bool cancelled(void *context)
{
    lane_t *lane = context;
    return atomic_load(&stopping) || atomic_load(&lane->closing);
}
static void close_lane(lane_t *lane)
{
    /* Remove the pointer before freeing it. Dispatcher cancellation reads the
     * pointer under the same short lock; no queue/socket wait is inside it. */
    portENTER_CRITICAL(&mux);
    app_wifi_channel_t *channel = lane->channel;
    lane->channel = NULL;
    portEXIT_CRITICAL(&mux);
    if (channel) app_wifi_channel_close(&channel); /* sole lane owner, no active I/O */
    portENTER_CRITICAL(&mux);
    if (lane->reserved) {
        closed_tokens[closed_index++ % 8] = (closed_token_t){lane->token, lane->owner_generation, lane->owner};
    }
    lane->reserved = false;
    portEXIT_CRITICAL(&mux);
}
static esp_err_t complete(app_message_t *request, app_wifi_result_t result, size_t bytes)
{
    app_message_t reply = {0};
    reply.payload.channel = request->payload.channel;
    reply.payload.channel.length = bytes;
    reply.payload.channel.status = io_status(result);
    if (result != APP_WIFI_OK) reply.payload.channel.readable = false;
    reply.result = error_code(reply.payload.channel.status);
    /* Transfer, never duplicate, the lease. A timed-out/old requester causes
     * router reply rejection and the router releases this reference. */
    reply.lease = request->lease;
    request->lease = NULL;
    esp_err_t error = app_console_reply(request, &reply);
    app_message_release(&reply);
    app_message_release(request);
    return error;
}
static void execute(lane_t *lane, app_message_t *request)
{
    app_wifi_result_t result = APP_WIFI_CANCELLED;
    size_t bytes = 0;
    /* Slot tokens are never reused while queued operations remain. */
    portENTER_CRITICAL(&mux);
    bool matches = lane->reserved && lane->token == request->payload.channel.token;
    portEXIT_CRITICAL(&mux);
    if (!matches) { complete(request, APP_WIFI_STALE, 0); return; }
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_CLOSE) {
        app_message_t queued = {0};
        while (xQueueReceive(lane->jobs, &queued, 0) == pdTRUE)
            complete(&queued, APP_WIFI_CANCELLED, 0);
        close_lane(lane);
        complete(request, APP_WIFI_OK, 0);
        return;
    }
    app_wifi_deadline_t deadline = {.at_us = request->deadline_us,
        .generation = request->payload.channel.generation, .cancelled = cancelled, .context = lane};
    if (!cancelled(lane) && esp_timer_get_time() >= deadline.at_us) result = APP_WIFI_TIMEOUT;
    else if (!cancelled(lane)) {
        if (request->type == APP_MESSAGE_WIFI_CHANNEL_OPEN) {
            app_wifi_endpoint_t endpoint = {.port = request->payload.channel.port};
            memcpy(endpoint.address, request->payload.channel.address, sizeof(endpoint.address));
            app_wifi_channel_t *channel = NULL;
            result = app_wifi_channel_connect(network, &endpoint, &deadline, &channel);
            portENTER_CRITICAL(&mux);
            lane->channel = channel;
            if (atomic_load(&lane->closing) && channel) app_wifi_channel_cancel(channel);
            portEXIT_CRITICAL(&mux);
            /* Once cancellation raced with successful open, do not publish an
             * apparently live token. Close the newly acquired ownership. */
            if (result == APP_WIFI_OK && cancelled(lane)) result = APP_WIFI_CANCELLED;
        } else if (request->type == APP_MESSAGE_WIFI_CHANNEL_RECEIVE && request->payload.channel.poll) {
            result = app_wifi_channel_poll(lane->channel, &deadline, &request->payload.channel.readable);
        } else {
            size_t capacity = 0;
            if (request->type == APP_MESSAGE_WIFI_CHANNEL_SEND) {
                const void *data = app_message_lease_data(request->lease, &capacity);
                result = !data || request->payload.channel.length > capacity ? APP_WIFI_INVALID :
                    app_wifi_channel_send(lane->channel, data, request->payload.channel.length, &deadline, &bytes);
            } else {
                void *data = app_message_lease_write(request->lease, &capacity);
                result = !data || request->payload.channel.length > capacity ? APP_WIFI_INVALID :
                    app_wifi_channel_receive(lane->channel, data, request->payload.channel.length, &deadline, &bytes);
            }
        }
    }
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_OPEN && result != APP_WIFI_OK) close_lane(lane);
    esp_err_t delivered = complete(request, result, bytes);
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_OPEN && result == APP_WIFI_OK && delivered != ESP_OK)
        close_lane(lane); /* no token owner exists for a rejected/late OPEN reply */
}
static void worker(void *context)
{
    lane_t *lane = context;
    for (;;) {
        app_message_t request = {0};
        if (xQueueReceive(lane->control, &request, 0) == pdTRUE ||
            xQueueReceive(lane->jobs, &request, pdMS_TO_TICKS(10)) == pdTRUE) {
            execute(lane, &request);
            continue;
        }
        if (atomic_load(&stopping)) break;
    }
    close_lane(lane);
    atomic_fetch_sub(&workers, 1);
    vTaskDelete(NULL);
}
void wifi_channel_messages_cancel(void)
{
    atomic_store(&stopping, true);
    portENTER_CRITICAL(&mux);
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        atomic_store(&lanes[i].closing, true);
        if (lanes[i].channel) app_wifi_channel_cancel(lanes[i].channel);
    }
    portEXIT_CRITICAL(&mux);
}
bool wifi_channel_messages_idle(void) { return atomic_load(&workers) == 0; }
void wifi_channel_messages_cleanup(void)
{
    if (!wifi_channel_messages_idle()) return;
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        if (lanes[i].jobs) vQueueDelete(lanes[i].jobs);
        if (lanes[i].control) vQueueDelete(lanes[i].control);
        lanes[i].jobs = lanes[i].control = NULL;
    }
    network = NULL;
}
esp_err_t wifi_channel_messages_start(app_wifi_t *wifi)
{
    if (network || !wifi || !wifi_channel_messages_idle()) return ESP_ERR_INVALID_STATE;
    network = wifi;
    atomic_store(&stopping, false);
    if (!(app_wifi_capabilities(wifi) & APP_WIFI_CAP_TCP)) return ESP_OK;
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        lanes[i].jobs = xQueueCreate(4, sizeof(app_message_t));
        lanes[i].control = xQueueCreate(1, sizeof(app_message_t));
        if (!lanes[i].jobs || !lanes[i].control) {
            wifi_channel_messages_cleanup(); return ESP_ERR_NO_MEM;
        }
    }
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        atomic_fetch_add(&workers, 1);
        if (xTaskCreate(worker, i ? "wifi_tcp_event" : "wifi_tcp_command", 4096, &lanes[i], 2, NULL) != pdPASS) {
            atomic_fetch_sub(&workers, 1);
            wifi_channel_messages_cancel();
            while (!wifi_channel_messages_idle()) vTaskDelay(1);
            wifi_channel_messages_cleanup(); return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}
esp_err_t wifi_channel_messages_dispatch(app_message_t *message)
{
    if (!network || atomic_load(&stopping)) return ESP_ERR_INVALID_STATE;
    if (!(app_wifi_capabilities(network) & APP_WIFI_CAP_TCP)) return ESP_ERR_NOT_SUPPORTED;
    if (message->type < APP_MESSAGE_WIFI_CHANNEL_OPEN || message->type > APP_MESSAGE_WIFI_CHANNEL_CLOSE ||
        !(message->flags & APP_MESSAGE_REQUEST) || message->deadline_us <= 0 ||
        message->source <= APP_ENDPOINT_NONE || message->source >= APP_ENDPOINT_COUNT) return ESP_ERR_INVALID_ARG;
    bool opening = message->type == APP_MESSAGE_WIFI_CHANNEL_OPEN;
    bool closing = message->type == APP_MESSAGE_WIFI_CHANNEL_CLOSE;
    bool polling = message->type == APP_MESSAGE_WIFI_CHANNEL_RECEIVE && message->payload.channel.poll;
    if (polling && (message->payload.channel.length || (message->flags & APP_MESSAGE_BULK))) return ESP_ERR_INVALID_ARG;
    if (!opening && !closing && !polling && (!message->lease || !(message->flags & APP_MESSAGE_BULK))) return ESP_ERR_INVALID_ARG;
    if ((opening || closing || polling) && message->lease) return ESP_ERR_INVALID_ARG;
    app_wifi_status_t status = {0};
    if (opening && (app_wifi_get_status(network, &status) != APP_WIFI_OK || !status.online || !status.generation))
        return ESP_ERR_INVALID_STATE;
    if (opening && message->payload.channel.generation && message->payload.channel.generation != status.generation)
        return ESP_ERR_INVALID_STATE;
    lane_t *lane = NULL;
    bool already_closed = false;
    portENTER_CRITICAL(&mux);
    for (unsigned i = 0; i < CHANNEL_COUNT; ++i) {
        if ((opening && !lanes[i].reserved) || (!opening && lanes[i].reserved &&
            (lanes[i].token == message->payload.channel.token || (closing && !message->payload.channel.token &&
                message->payload.channel.opening_correlation &&
                lanes[i].opening_correlation == message->payload.channel.opening_correlation)) &&
            lanes[i].owner == message->source &&
            lanes[i].owner_generation == message->generation)) { lane = &lanes[i]; break; }
    }
    if (lane && opening) {
        lane->reserved = true;
        lane->owner = message->source;
        lane->owner_generation = message->generation;
        lane->opening_correlation = message->correlation_id;
        lane->network_generation = status.generation;
        if (!++next_token) ++next_token;
        lane->token = next_token;
        atomic_store(&lane->closing, false);
        message->payload.channel.token = lane->token;
        message->payload.channel.generation = status.generation;
    } else if (lane && closing) {
        message->payload.channel.token = lane->token;
        atomic_store(&lane->closing, true);
        if (lane->channel) app_wifi_channel_cancel(lane->channel);
    } else if (lane && (atomic_load(&lane->closing) ||
        message->payload.channel.generation != lane->network_generation)) lane = NULL;
    if (!lane && closing && message->payload.channel.token) {
        for (unsigned i = 0; i < 8; ++i)
            if (closed_tokens[i].token == message->payload.channel.token &&
                closed_tokens[i].owner == message->source && closed_tokens[i].generation == message->generation)
                already_closed = true;
    }
    portEXIT_CRITICAL(&mux);
    if (already_closed) { complete(message, APP_WIFI_OK, 0); return ESP_OK; }
    if (!lane) return opening ? ESP_ERR_NO_MEM : ESP_ERR_INVALID_STATE;
    if (xQueueSend(closing ? lane->control : lane->jobs, message, 0) != pdTRUE) {
        if (opening) close_lane(lane);
        return ESP_ERR_TIMEOUT;
    }
    /* Value-copy into the queue transfers exactly one lease reference. */
    message->lease = NULL;
    return ESP_OK;
}
