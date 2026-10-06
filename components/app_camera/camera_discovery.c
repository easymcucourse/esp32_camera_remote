#include "camera_discovery.h"
#include "camera_link.h"
#include "app_console.h"
#include "esp_timer.h"
#include <string.h>

static camera_backend_result_t network_result(esp_err_t result)
{
    switch (result) {
    case ESP_OK: return CAMERA_BACKEND_OK;
    case ESP_ERR_TIMEOUT: return CAMERA_BACKEND_TIMEOUT;
    case ESP_ERR_INVALID_STATE: return CAMERA_BACKEND_CANCELLED;
    default: return CAMERA_BACKEND_NETWORK;
    }
}
static app_message_t request(app_message_type_t type, uint32_t generation)
{
    return (app_message_t){.source = APP_ENDPOINT_CAMERA, .target = APP_ENDPOINT_WIFI,
        .type = type, .flags = APP_MESSAGE_REQUEST, .generation = generation,
        .deadline_us = esp_timer_get_time() + 1000000};
}
camera_backend_result_t camera_network_select(uint32_t generation, const uint8_t mac[6])
{
    if (!generation) return CAMERA_BACKEND_INVALID;
    app_message_t message = request(APP_MESSAGE_WIFI_SELECT_CAMERA, generation), reply = {0};
    message.payload.peer.selected = mac != NULL;
    if (mac) memcpy(message.payload.peer.mac, mac, 6);
    esp_err_t error = app_console_request(&message, &reply);
    if (error == ESP_OK) error = reply.result;
    app_message_release(&reply); return network_result(error);
}
camera_backend_result_t camera_discovery_scan(camera_session_t *probe,
    uint32_t generation, bool paired, const uint8_t saved_mac[6], camera_discovery_t *out)
{
    if (!probe || !probe->factory || !out || !generation || (paired && !saved_mac)) return CAMERA_BACKEND_INVALID;
    *out = (camera_discovery_t){.selected = -1};
    if (probe->backend || probe->state != CAMERA_SESSION_EMPTY) return CAMERA_BACKEND_STATE;
    app_message_t message = request(APP_MESSAGE_CAMERA_DISCOVER, generation), reply = {0};
    esp_err_t error = app_console_request_cancelable(&message, &reply, probe->cancelled, probe->cancel_context);
    if (error == ESP_OK) error = reply.result;
    if (error != ESP_OK) { app_message_release(&reply); return network_result(error); }
    app_network_client_t clients[4]; size_t count = reply.payload.discovery.count;
    if (count > 4) { app_message_release(&reply); return CAMERA_BACKEND_PROTOCOL; }
    memcpy(clients, reply.payload.discovery.clients, count * sizeof *clients);
    app_message_release(&reply);
    uint32_t reachable = 0;
    for (size_t i = 0; i < count; ++i) {
        if (!camera_candidate_allowed(paired, saved_mac, clients[i].mac)) continue;
        if (!clients[i].ip[0] || !memchr(clients[i].ip, 0, sizeof clients[i].ip)) return CAMERA_BACKEND_PROTOCOL;
        if (probe->cancelled && probe->cancelled(probe->cancel_context)) return CAMERA_BACKEND_CANCELLED;
        out->connect_failed = true;
        camera_backend_result_t result = camera_session_probe(probe, clients[i].ip, 15740, 800);
        bool connected = result == CAMERA_BACKEND_OK;
        camera_backend_result_t cleanup = camera_session_close(probe, 1000);
        if (cleanup != CAMERA_BACKEND_OK) return cleanup;
        if (connected) reachable |= 1u << i;
        else if (result == CAMERA_BACKEND_CANCELLED) return result;
        else if (result != CAMERA_BACKEND_NETWORK && result != CAMERA_BACKEND_TIMEOUT &&
            result != CAMERA_BACKEND_REFUSED) return result;
    }
    int selected = camera_select_candidate(reachable);
    if (probe->cancelled && probe->cancelled(probe->cancel_context)) return CAMERA_BACKEND_CANCELLED;
    if (selected >= 0) {
        camera_backend_result_t result = camera_network_select(generation, clients[selected].mac);
        if (result != CAMERA_BACKEND_OK) return result;
        out->peer = clients[selected]; out->connect_failed = false;
    } else if (selected == -2) out->connect_failed = false;
    out->selected = selected;
    return CAMERA_BACKEND_OK;
}
