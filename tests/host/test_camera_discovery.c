#include "camera_discovery.h"
#include "app_console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static camera_backend_t object;
static bool alive, stopped, close_failed, stop_after_probe;
static unsigned creates, probes, closes, destroys, selects, releases, count = 4;
static uint32_t reachable, generations[16];
static app_network_client_t clients[4];
static uint8_t selected_mac[6];
static esp_err_t rpc_result;
int64_t esp_timer_get_time(void) { return 1000; }
void app_message_release(app_message_t *message) { assert(!message->lease); ++releases; }
static esp_err_t exchange(app_message_t *request, app_message_t *reply)
{
    assert(request->source == APP_ENDPOINT_CAMERA && request->target == APP_ENDPOINT_WIFI);
    assert(request->generation == 9 && request->deadline_us == 1001000 && !request->lease);
    if (rpc_result != ESP_OK) return rpc_result;
    if (request->type == APP_MESSAGE_CAMERA_DISCOVER) {
        reply->payload.discovery.count = count;
        memcpy(reply->payload.discovery.clients, clients, sizeof clients);
    } else {
        assert(request->type == APP_MESSAGE_WIFI_SELECT_CAMERA); ++selects;
        memcpy(selected_mac, request->payload.peer.mac, 6);
        if (!request->payload.peer.selected) memset(selected_mac, 0, sizeof selected_mac);
    }
    return ESP_OK;
}
esp_err_t app_console_request(app_message_t *request, app_message_t *reply) { return exchange(request, reply); }
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn predicate, void *context)
{ if (predicate && predicate(context)) return ESP_ERR_INVALID_STATE; return exchange(request, reply); }
static bool cancelled(void *context) { assert(context == &stopped); return stopped; }
static camera_backend_result_t probe(void *context, const char *address, uint16_t port, uint32_t timeout)
{
    assert(context == &alive && alive && port == 15740 && timeout == 800); ++probes;
    unsigned i = address[strlen(address) - 1] - '1'; assert(i < 4);
    if (stop_after_probe) stopped = true;
    return reachable & (1u << i) ? CAMERA_BACKEND_OK : CAMERA_BACKEND_NETWORK;
}
static camera_backend_result_t connect_camera(void *c, const camera_connection_t *target,
    void *scratch, size_t n, uint32_t t, camera_peer_t *peer)
{ (void)c; (void)target; (void)scratch; (void)n; (void)t; (void)peer; assert(false); return CAMERA_BACKEND_INVALID; }
static camera_backend_result_t close_camera(void *context, uint32_t timeout)
{ assert(context == &alive && alive && timeout == 1000); ++closes; return close_failed ? CAMERA_BACKEND_TIMEOUT : CAMERA_BACKEND_OK; }
static camera_backend_result_t destroy_camera(void *context)
{ assert(context == &alive && alive); alive = false; ++destroys; return CAMERA_BACKEND_OK; }
static void cancel_camera(void *c) { (void)c; }
static void network_camera(void *c, uint32_t g) { (void)c; (void)g; }
static const camera_backend_ops_t ops = {.api_version = CAMERA_BACKEND_API_VERSION,
    .capabilities = CAMERA_BACKEND_CAP_PROBE, .probe = probe, .connect = connect_camera,
    .disconnect = close_camera, .destroy = destroy_camera, .cancel = cancel_camera, .network_changed = network_camera};
static camera_backend_result_t create(uint32_t generation, bool (*predicate)(void *), void *context, camera_backend_t **out)
{
    assert(!alive && !*out && generation && predicate == cancelled && context == &stopped);
    generations[creates++] = generation; assert(creates <= 16);
    object = (camera_backend_t){0}; alive = true;
    assert(camera_backend_bind(&object, &ops, &alive, CAMERA_BACKEND_CAP_PROBE) == CAMERA_BACKEND_OK);
    *out = &object; return CAMERA_BACKEND_OK;
}
static const camera_backend_factory_t factory = {.api_version = CAMERA_FACTORY_API_VERSION,
    .required_capabilities = CAMERA_BACKEND_CAP_PROBE, .create = create};
int main(void)
{
    for (unsigned i = 0; i < 4; ++i) {
        snprintf(clients[i].ip, sizeof clients[i].ip, "192.168.4.%u", i + 1);
        clients[i].mac[5] = i + 1; clients[i].rssi = -40 - (int)i;
    }
    camera_session_t session = {0}; camera_discovery_t result;
    assert(camera_session_init(&session, &factory, cancelled, &stopped) == CAMERA_BACKEND_OK);
    reachable = 1u << 2;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_OK);
    assert(result.selected == 2 && !result.connect_failed && result.peer.mac[5] == 3 && result.peer.rssi == -42);
    assert(selects == 1 && probes == 4 && closes == 4 && destroys == 4 && !alive && !session.backend);
    for (unsigned i = 1; i < creates; ++i) assert(generations[i] != generations[i - 1]);
    reachable = 3;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_OK);
    assert(result.selected == -2 && !result.connect_failed && selects == 1 && !session.backend);
    unsigned before = probes; reachable = 1;
    assert(camera_discovery_scan(&session, 9, true, clients[0].mac, &result) == CAMERA_BACKEND_OK);
    assert(result.selected == 0 && probes == before + 1 && selected_mac[5] == 1);
    reachable = 0;
    assert(camera_discovery_scan(&session, 9, true, clients[0].mac, &result) == CAMERA_BACKEND_OK);
    assert(result.selected == -1 && result.connect_failed);
    count = 0; before = probes;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_OK);
    assert(result.selected == -1 && !result.connect_failed && probes == before);
    count = 5; assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_PROTOCOL && probes == before);
    count = 1; rpc_result = ESP_ERR_TIMEOUT;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_TIMEOUT && probes == before);
    rpc_result = ESP_OK; close_failed = true;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_TIMEOUT && alive && session.backend);
    before = probes;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_STATE && probes == before);
    close_failed = false; assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK && !alive);
    stop_after_probe = true;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_CANCELLED && !alive && !session.backend);
    stop_after_probe = stopped = false;
    memset(clients[0].ip, 'x', sizeof clients[0].ip); before = probes;
    assert(camera_discovery_scan(&session, 9, false, NULL, &result) == CAMERA_BACKEND_PROTOCOL && probes == before);
    assert(camera_network_select(9, NULL) == CAMERA_BACKEND_OK && !selected_mac[5]);
    assert(releases > 0);
    return 0;
}
