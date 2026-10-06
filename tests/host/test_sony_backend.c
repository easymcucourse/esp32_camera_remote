/* Reuse the complete PTP fake Console fixture and its original protocol
 * regression. The backend links the same real client/wire, without Wi-Fi/fds. */
#define main ptp_regression_main
#define app_console_request_cancelable fixture_protocol_rpc
#define app_console_request fixture_unused_request
#include "test_ptpip_protocol.c"
#undef main
#undef app_console_request_cancelable
#undef app_console_request
#include "camera_backend_sony_factory.h"
static bool backend_active, close_failed;
static unsigned opens, closes;
esp_err_t app_console_request(app_message_t *request, app_message_t *reply)
{
    assert(backend_active && request->type == APP_MESSAGE_WIFI_CHANNEL_CLOSE);
    ++closes; reply->payload.channel = request->payload.channel;
    if (close_failed) { reply->result = ESP_ERR_TIMEOUT; return ESP_OK; }
    return ESP_OK;
}
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn predicate, void *context)
{
    if (!backend_active) return fixture_protocol_rpc(request, reply, predicate, context);
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_OPEN) {
        assert(!predicate(context)); ++opens;
        reply->payload.channel.token = opens == 1 ? 42 : 43;
        reply->payload.channel.generation = 7;
        return ESP_OK;
    }
    expected_token = request->payload.channel.token;
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_SEND) {
        size_t size; const uint8_t *data = app_message_lease_data(request->lease, &size);
        if (size >= 8 && (get32(data + 4) == PTPIP_OPERATION_REQUEST ||
            get32(data + 4) == PTPIP_INIT_COMMAND_REQUEST || get32(data + 4) == PTPIP_INIT_EVENT_REQUEST))
            deadline = 0;
    }
    return fixture_protocol_rpc(request, reply, predicate, context);
}
static void data_packet(uint32_t transaction, const uint8_t *data, size_t size)
{
    uint8_t *b = packet(PTPIP_START_DATA, 20, transaction); put32(b + 12, size);
    b = packet(PTPIP_END_DATA, 12 + size, transaction);
    memcpy(b + 12, data, size); response(transaction, PTP_RC_OK);
}
static size_t string(uint8_t *data, const char *text)
{
    size_t n = strlen(text) + 1; *data = n;
    for (size_t i = 0; i < n; ++i) { data[1 + i*2] = text[i]; data[2 + i*2] = 0; }
    return 1 + n*2;
}
static unsigned observed;
static void property(void *context, const camera_property_t *p)
{
    assert(context == &observed); ++observed;
    if (p->setting == CAMERA_SETTING_MODE) {
        assert(p->writable && !p->relative && p->current.type == CAMERA_VALUE_U32 && p->current.bits == 2);
        assert(p->choice_count == 3 && p->choices[2].bits == 3);
    } else if (p->setting == CAMERA_SETTING_EV) {
        assert(p->current.type == CAMERA_VALUE_I16 && p->current.bits == 0xffff && p->choice_count == 3);
        assert(p->choices[0].bits == 0xff00 && p->choices[1].bits == 0xffff && p->choices[2].bits == 0);
    } else assert(p->setting == CAMERA_SETTING_APERTURE && p->writable && p->relative && p->current.bits == 400);
}
static void backend_reset(void) { reset(); expected_token = 42; }
int main(void)
{
    assert(ptp_regression_main() == 0);
    backend_active = true; backend_reset();
    camera_backend_t *backend = NULL;
    assert(camera_backend_sony_create(0, NULL, NULL, &backend) == CAMERA_BACKEND_INVALID && !backend);
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_INVALID);
    camera_connection_t connection = {.address = "192.168.4.2", .port = 15740,
        .local_name = "ESP32-Camera-Remote", .handshake_timeout_ms = 10000};
    uint8_t scratch[1024], info[128] = {0}, props[48] = {0}; size_t n = 8;
    n += string(info + n, "Sony"); n += 2 + 5*4;
    n += string(info + n, "Sony Corporation"); n += string(info + n, "ZV-E10"); n += string(info + n, "2.00");
    put32(props, 2); uint8_t *b = props + 8;
    word(b, SONY_DPC_EXPOSURE_PROGRAM); word(b + 2, 6); b[4] = b[5] = 1;
    put32(b + 6, 1); put32(b + 10, 2); b[14] = 2; b[15] = 3;
    put32(b + 17, 1); put32(b + 21, 2); put32(b + 25, 3);
    b = props + 37; word(b, SONY_DPC_F_NUMBER); word(b + 2, 4); b[4] = b[5] = 1;
    word(b + 6, 280); word(b + 8, 400);
    initial_ack(); packet(PTPIP_INIT_EVENT_ACK, 8, 0); response(2, PTP_RC_OK);
    uint8_t empty = 0;
    data_packet(3, &empty, 0); data_packet(4, &empty, 0); data_packet(5, info, n);
    data_packet(6, &empty, 0); data_packet(7, &empty, 0); data_packet(8, &empty, 0);
    data_packet(9, props, sizeof props); response(10, PTP_RC_ACCESS_DENIED);
    camera_peer_t peer = {0};
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_OK);
    assert(opens == 2 && !strcmp(peer.model, "ZV-E10") && !strcmp(peer.firmware, "2.00") && peer.guid[0] == 0x42);
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_STATE);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_STATE);
    /* Standard initialization allocated 2..10; vendor controls continue at11. */
    backend_reset(); response(11, PTP_RC_OK);
    assert(backend->ops->set(backend->context, CAMERA_SETTING_MODE, (camera_value_t){CAMERA_VALUE_U32, 99}, 5000) == CAMERA_BACKEND_INVALID && !calls);
    assert(backend->ops->set(backend->context, CAMERA_SETTING_MODE, (camera_value_t){CAMERA_VALUE_U32, 3}, 5000) == CAMERA_BACKEND_OK);
    assert(get32(sent + 14) == 11 && get32(sent + 18) == SONY_DPC_EXPOSURE_PROGRAM);
    backend_reset(); response(12, PTP_RC_OK);
    assert(backend->ops->step(backend->context, CAMERA_SETTING_APERTURE, -1, 5000) == CAMERA_BACKEND_OK);
    assert(get32(sent + 14) == 12 && get32(sent + 18) == SONY_DPC_F_NUMBER && sent[54] == 255);
    backend_reset(); response(13, PTP_RC_OK);
    assert(backend->ops->action(backend->context, CAMERA_ACTION_RECORD, 1, 5000) == CAMERA_BACKEND_OK);
    assert(get32(sent + 14) == 13 && get32(sent + 18) == SONY_DPC_MOVIE_RECORD);
    uint8_t ev_props[67]; memcpy(ev_props, props, sizeof props); put32(ev_props, 3);
    b = ev_props + 48; memset(b, 0, 19); word(b, SONY_DPC_EXPOSURE_BIAS); word(b + 2, 3); b[4] = b[5] = 1;
    word(b + 8, 0xffff); b[10] = 2; word(b + 11, 3);
    word(b + 13, 0xffff); word(b + 15, 0); word(b + 17, 0xff00);
    backend_reset(); data_packet(14, ev_props, sizeof ev_props); camera_capabilities_t caps;
    assert(backend->ops->properties(backend->context, scratch, sizeof scratch, 5000, property, &observed, &caps) == CAMERA_BACKEND_OK);
    assert(observed == 3 && !caps.focus_known && !caps.zoom_known && !caps.recording_known);
    backend_reset(); uint8_t object_data[160] = {0}; put32(object_data, 136);
    const uint8_t jpeg[] = {255,216,255,218,0,2,1,255,217}; memcpy(object_data + 136, jpeg, sizeof jpeg);
    data_packet(15, object_data, sizeof object_data); camera_frame_t frame;
    assert(backend->ops->liveview(backend->context, scratch, sizeof scratch, 5000, &frame) == CAMERA_BACKEND_OK);
    assert(frame.jpeg == scratch + 136 && frame.size == sizeof jpeg);
    backend_reset(); response(16, PTP_RC_ACCESS_DENIED);
    assert(backend->ops->liveview(backend->context, scratch, sizeof scratch, 5000, &frame) == CAMERA_BACKEND_NOT_READY && !frame.jpeg);
    backend_reset(); b = packet(PTPIP_EVENT, 18, 0); word(b + 8, 0xc203); put32(b + 10, 16);
    bool changed;
    assert(backend->ops->events(backend->context, 5000, &changed) == CAMERA_BACKEND_OK && changed);
    backend_reset(); data_packet(17, props, sizeof props - 1);
    assert(backend->ops->properties(backend->context, scratch, sizeof scratch, 5000, property, &observed, &caps) == CAMERA_BACKEND_PROTOCOL);
    assert(observed == 3);
    backend_reset();
    assert(backend->ops->set(backend->context, CAMERA_SETTING_MODE, (camera_value_t){CAMERA_VALUE_U32, 1}, 5000) == CAMERA_BACKEND_UNSUPPORTED && !calls);
    backend->ops->network_changed(backend->context, 8);
    assert(backend->ops->action(backend->context, CAMERA_ACTION_RECORD, 0, 5000) == CAMERA_BACKEND_NETWORK && !calls);
    backend->ops->cancel(backend->context);
    assert(backend->ops->action(backend->context, CAMERA_ACTION_RECORD, 0, 5000) == CAMERA_BACKEND_CANCELLED && !calls);
    close_failed = true;
    assert(backend->ops->disconnect(backend->context, 1000) != CAMERA_BACKEND_OK);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_STATE);
    close_failed = false;
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK && closes == 4);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK);
    /* Saved peer rejection happens before Event OPEN/OpenSession/vendor I/O;
     * failed connect owns its command token until explicit cleanup succeeds. */
    backend = NULL; opens = 0; backend_reset(); initial_ack();
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    connection.require_peer_guid = true;
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_IDENTITY);
    assert(opens == 1 && backend->ops->destroy(backend->context) == CAMERA_BACKEND_STATE);
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK);
    /* Healthy shutdown sends the original standard CloseSession before tokens
     * are released, using the same transaction owner (next11). */
    backend = NULL; opens = 0; backend_reset(); connection.require_peer_guid = false;
    initial_ack(); packet(PTPIP_INIT_EVENT_ACK, 8, 0); response(2, PTP_RC_OK);
    data_packet(3, &empty, 0); data_packet(4, &empty, 0); data_packet(5, info, n);
    data_packet(6, &empty, 0); data_packet(7, &empty, 0); data_packet(8, &empty, 0);
    data_packet(9, props, sizeof props); response(10, PTP_RC_ACCESS_DENIED);
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_OK);
    backend_reset(); response(11, PTP_RC_OK);
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK);
    assert(get16(sent + 12) == PTP_OC_CLOSE_SESSION && get32(sent + 14) == 11);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK);
    backend = NULL; opens = 0; backend_reset();
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    assert(backend->capabilities & CAMERA_BACKEND_CAP_PROBE);
    assert(backend->ops->probe(backend->context, "192.168.4.2", 15740, 800) == CAMERA_BACKEND_OK);
    assert(opens == 1 && !calls && !sent_size); /* TCP only, no protocol transaction. */
    assert(backend->ops->probe(backend->context, "192.168.4.2", 15740, 800) == CAMERA_BACKEND_STATE && opens == 1);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_STATE);
    backend->ops->cancel(backend->context);
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK && !sent_size);
    /* Explicit InitFail remains an identity/pairing rejection even with reason
     * zero. Owner must wait for user action instead of reconnecting forever. */
    backend = NULL; opens = 0; backend_reset(); packet(PTPIP_INIT_FAIL, 12, 0);
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_IDENTITY);
    assert(opens == 1 && backend->ops->destroy(backend->context) == CAMERA_BACKEND_STATE);
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK);
    backend = NULL; opens = 0; backend_reset();
    initial_ack(); packet(PTPIP_INIT_EVENT_ACK, 8, 0); response(2, PTP_RC_OK);
    data_packet(3, &empty, 0); data_packet(4, &empty, 0); data_packet(5, info, n);
    data_packet(6, &empty, 0); data_packet(7, &empty, 0); data_packet(8, &empty, 0);
    data_packet(9, props, sizeof props); response(10, PTP_RC_ACCESS_DENIED);
    assert(camera_backend_sony_create(9, NULL, NULL, &backend) == CAMERA_BACKEND_OK);
    assert(backend->ops->connect(backend->context, &connection, scratch, sizeof scratch, 130000, &peer) == CAMERA_BACKEND_OK);
    backend_reset(); response(11, 0x2009);
    assert(backend->ops->liveview(backend->context, scratch, sizeof scratch, 5000, &frame) == CAMERA_BACKEND_REFUSED && !frame.jpeg);
    backend_reset(); response(12, PTP_RC_ACCESS_DENIED);
    assert(backend->ops->liveview(backend->context, scratch, sizeof scratch, 5000, &frame) == CAMERA_BACKEND_NOT_READY && !frame.jpeg);
    backend_reset(); response(13, PTP_RC_OK);
    assert(backend->ops->disconnect(backend->context, 1000) == CAMERA_BACKEND_OK);
    assert(backend->ops->destroy(backend->context) == CAMERA_BACKEND_OK);
    return 0;
}
