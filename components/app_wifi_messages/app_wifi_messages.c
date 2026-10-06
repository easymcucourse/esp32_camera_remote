#include "app_wifi_messages.h"
#include "app_console.h"
#include "private/wifi_channel_messages.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <string.h>

static atomic_bool running;
static app_wifi_t *network;
static portMUX_TYPE peer_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t selected_mac[6];
static bool selected;
void app_wifi_messages_select_camera(const uint8_t mac[6])
{
    portENTER_CRITICAL(&peer_mux);
    selected = mac != NULL;
    if (mac) memcpy(selected_mac, mac, 6);
    portEXIT_CRITICAL(&peer_mux);
}
static esp_err_t convert(app_wifi_result_t error)
{
    switch (error) {
    case APP_WIFI_OK: return ESP_OK;
    case APP_WIFI_INVALID: return ESP_ERR_INVALID_ARG;
    case APP_WIFI_STATE: case APP_WIFI_CANCELLED: return ESP_ERR_INVALID_STATE;
    case APP_WIFI_NO_MEMORY: return ESP_ERR_NO_MEM;
    case APP_WIFI_TIMEOUT: return ESP_ERR_TIMEOUT;
    case APP_WIFI_UNSUPPORTED: return ESP_ERR_NOT_SUPPORTED;
    case APP_WIFI_PENDING: return ESP_ERR_NOT_FINISHED;
    case APP_WIFI_NOT_FOUND: return ESP_ERR_NOT_FOUND;
    default: return ESP_FAIL;
    }
}
static void copy_config(app_network_config_t *out, const network_config_t *config)
{
    *out = (app_network_config_t){.channel = config->channel,
        .show_password = config->show_password, .default_password = network_config_uses_default_password(config)};
    memcpy(out->ssid, config->ssid, sizeof(config->ssid));
    memcpy(out->password, config->password, sizeof(config->password));
}
static esp_err_t snapshot(app_message_t *reply)
{
    app_wifi_status_t status = {0};
    app_wifi_result_t result = app_wifi_get_status(network, &status);
    reply->payload.network.online = status.online;
    reply->payload.network.generation = status.generation;
    reply->payload.network.max_channel = status.max_channel;
    memcpy(reply->payload.network.address, status.address, sizeof(status.address));
    network_config_t config;
    if (app_wifi_config_get(network, &config) == APP_WIFI_OK) copy_config(&reply->payload.network.config, &config);
    return convert(result);
}
static esp_err_t handle(const app_message_t *message, app_message_t *reply)
{
    if (message->type == APP_MESSAGE_WIFI_STATUS) {
        return snapshot(reply);
    }
    if (message->type == APP_MESSAGE_WIFI_CONFIG_GET) {
        network_config_t config; app_wifi_result_t result = app_wifi_config_get(network, &config);
        if (result == APP_WIFI_OK) copy_config(&reply->payload.config, &config);
        return convert(result);
    }
    if (message->type == APP_MESSAGE_WIFI_SELECT_CAMERA) {
        app_wifi_messages_select_camera(message->payload.peer.selected ? message->payload.peer.mac : NULL);
        return ESP_OK;
    }
    if (message->type == APP_MESSAGE_CAMERA_DISCOVER || message->type == APP_MESSAGE_WIFI_RSSI) {
        app_wifi_client_t clients[APP_WIFI_CLIENT_CAPACITY]; size_t count = 0;
        app_wifi_result_t result = app_wifi_get_clients(network, clients, APP_WIFI_CLIENT_CAPACITY, &count);
        if (result != APP_WIFI_OK) return convert(result);
        if (message->type == APP_MESSAGE_WIFI_RSSI) {
            memcpy(reply->payload.peer.mac, message->payload.peer.mac, 6);
            reply->payload.peer.rssi = -127;
            for (size_t i = 0; i < count; ++i) if (!memcmp(clients[i].mac, message->payload.peer.mac, 6))
                reply->payload.peer.rssi = clients[i].rssi;
        } else {
            reply->payload.discovery.count = count;
            for (size_t i = 0; i < count; ++i) {
                memcpy(reply->payload.discovery.clients[i].mac, clients[i].mac, 6);
                memcpy(reply->payload.discovery.clients[i].ip, clients[i].ip, sizeof(clients[i].ip));
                reply->payload.discovery.clients[i].rssi = clients[i].rssi;
            }
        }
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}
static void task(void *context)
{
    (void)context;
    int64_t last_state = esp_timer_get_time(), last_rssi = last_state;
    uint32_t last_generation = 0;
    while (atomic_load(&running)) {
        app_message_t message = {0}, reply = {0};
        esp_err_t error = app_console_receive(APP_ENDPOINT_WIFI, &message, 50);
        if (error == ESP_ERR_INVALID_STATE) break;
        if (error == ESP_OK) {
            bool dispatched = false;
            if (message.type >= APP_MESSAGE_WIFI_CHANNEL_OPEN && message.type <= APP_MESSAGE_WIFI_CHANNEL_CLOSE) {
                error = wifi_channel_messages_dispatch(&message);
                dispatched = error == ESP_OK;
                reply.result = error;
                reply.payload.channel = message.payload.channel;
                reply.payload.channel.length = 0;
                reply.payload.channel.readable = false;
                reply.payload.channel.status = error == ESP_ERR_TIMEOUT ? APP_NETWORK_IO_TIMEOUT :
                    error == ESP_ERR_INVALID_STATE ? APP_NETWORK_IO_STALE :
                    error == ESP_ERR_NO_MEM ? APP_NETWORK_IO_NO_MEMORY : APP_NETWORK_IO_INVALID;
                reply.lease = message.lease;
                message.lease = NULL;
            } else {
                reply.result = handle(&message, &reply);
            }
            if (!dispatched) {
                if (message.flags & APP_MESSAGE_REQUEST) app_console_reply(&message, &reply);
                if (reply.lease) app_message_release(&reply);
                app_message_release(&message);
            }
        }
        int64_t now = esp_timer_get_time();
        if (now - last_state >= 200000) {
            app_message_t state = {.type = APP_MESSAGE_WIFI_STATUS, .source = APP_ENDPOINT_WIFI, .flags = APP_MESSAGE_EVENT};
            if (snapshot(&state) == ESP_OK) {
                state.generation = state.payload.network.generation;
                if (state.generation != last_generation) {
                    app_message_t changed = {.type = APP_MESSAGE_WIFI_NETWORK_CHANGED, .source = APP_ENDPOINT_WIFI,
                        .flags = APP_MESSAGE_EVENT, .generation = state.generation};
                    changed.payload.network.generation = state.generation;
                    if (app_console_send(&changed) == ESP_OK) last_generation = state.generation;
                }
                app_console_send(&state);
            }
            last_state = now;
        }
        if (now - last_rssi >= 2000000) {
            uint8_t mac[6];
            portENTER_CRITICAL(&peer_mux); bool active = selected; memcpy(mac, selected_mac, 6); portEXIT_CRITICAL(&peer_mux);
            app_message_t request = {.type = APP_MESSAGE_WIFI_RSSI}, rssi = {
                .type = APP_MESSAGE_WIFI_RSSI, .source = APP_ENDPOINT_WIFI, .flags = APP_MESSAGE_EVENT};
            memcpy(request.payload.peer.mac, mac, 6);
            rssi.payload.peer.rssi = -127;
            if (active) handle(&request, &rssi);
            app_wifi_status_t status; app_wifi_get_status(network, &status); rssi.generation = status.generation;
            app_console_send(&rssi); last_rssi = now;
        }
    }
    wifi_channel_messages_cancel();
    while (!wifi_channel_messages_idle()) vTaskDelay(1);
    wifi_channel_messages_cleanup();
    network = NULL;
    atomic_store(&running, false);
    vTaskDelete(NULL);
}
esp_err_t app_wifi_messages_start(app_wifi_t *wifi)
{
    if (!wifi || app_wifi_api_version(wifi) != APP_WIFI_API_VERSION ||
        !(app_wifi_capabilities(wifi) & APP_WIFI_CAP_AP)) return ESP_ERR_INVALID_ARG;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&running, &expected, true)) return ESP_ERR_INVALID_STATE;
    const app_endpoint_config_t config = {16, 4};
    esp_err_t error = app_console_endpoint_register(APP_ENDPOINT_WIFI, &config);
    if (error != ESP_OK) { atomic_store(&running, false); return error; }
    error = wifi_channel_messages_start(wifi);
    if (error != ESP_OK) {
        atomic_store(&running, false); app_console_endpoint_stop(APP_ENDPOINT_WIFI); return error;
    }
    network = wifi;
    if (xTaskCreate(task, "wifi_endpoint", 4096, NULL, 2, NULL) != pdPASS) {
        wifi_channel_messages_cancel();
        while (!wifi_channel_messages_idle()) vTaskDelay(1);
        wifi_channel_messages_cleanup();
        network = NULL; atomic_store(&running, false);
        app_console_endpoint_stop(APP_ENDPOINT_WIFI); return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t app_wifi_messages_stop(uint32_t timeout_ms)
{
    app_console_endpoint_stop(APP_ENDPOINT_WIFI);
    wifi_channel_messages_cancel();
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (atomic_load(&running)) {
        if (esp_timer_get_time() >= deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(1);
    }
    return ESP_OK;
}
