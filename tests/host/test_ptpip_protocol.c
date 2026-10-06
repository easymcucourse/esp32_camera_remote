#include "ptpip_protocol.h"
#include "ptpip_packet.h"
#include "ptp_codes.h"
#include "sony_control_encoder.h"
#include "../support/legacy/sony_exposure_encoder.h"
#include "sony_codes.h"
#include "app_console.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>
static ptpip_client_t client;
static bool control_write(void *context, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted)
{ return ptpip_client_send_data(context, opcode, property, data, size, accepted); }
static sony_control_writer_t writer = {&client, control_write};
static uint8_t input[1024], sent[1024];
static size_t used, at, sent_size;
static int64_t now = 1000, deadline;
static unsigned calls;
static uint32_t expected_token = 42;
void fake_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
int64_t esp_timer_get_time(void) { return now; }
void vTaskDelay(TickType_t ticks) { (void)ticks; assert(false); }
esp_err_t app_console_send(app_message_t *message) { (void)message; assert(false); return ESP_FAIL; }
esp_err_t app_console_request(app_message_t *request, app_message_t *reply) { (void)request; (void)reply; assert(false); return ESP_FAIL; }
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn predicate, void *context)
{
    assert(!predicate(context)); ++calls;
    assert(request->source == APP_ENDPOINT_CAMERA && request->target == APP_ENDPOINT_WIFI && request->generation == 9);
    assert(request->payload.channel.token == expected_token && request->payload.channel.generation == 7);
    if (request->payload.channel.poll) {
        assert(request->flags == APP_MESSAGE_REQUEST && !request->lease && !request->payload.channel.length);
        reply->payload.channel = request->payload.channel;
        reply->payload.channel.readable = at < used; now += 100; deadline = 0;
        return ESP_OK;
    }
    assert(request->flags == (APP_MESSAGE_REQUEST | APP_MESSAGE_BULK));
    if (!deadline) deadline = request->deadline_us;
    assert(request->deadline_us == deadline && deadline > now);
    now += 100;
    size_t capacity = 0; const void *data = app_message_lease_data(request->lease, &capacity);
    assert(capacity == request->payload.channel.length);
    reply->payload.channel = request->payload.channel;
    reply->lease = request->lease; request->lease = NULL;
    if (request->type == APP_MESSAGE_WIFI_CHANNEL_SEND) {
        assert(!app_message_lease_write(reply->lease, NULL));
        assert(sent_size + capacity <= sizeof(sent));
        memcpy(sent + sent_size, data, capacity); sent_size += capacity;
    } else {
        void *output = app_message_lease_write(reply->lease, NULL); assert(output == data);
        if (capacity > used - at) {
            reply->payload.channel.length = 0; reply->payload.channel.status = APP_NETWORK_IO_CLOSED;
            reply->result = ESP_FAIL; return ESP_OK;
        }
        memcpy(output, input + at, capacity); at += capacity;
    }
    return ESP_OK;
}
static void word(uint8_t *b, uint16_t value) { b[0] = value; b[1] = value >> 8; }
static uint8_t *packet(uint32_t type, unsigned size, uint32_t transaction)
{
    uint8_t *b = input + used; memset(b, 0, size); put32(b, size); put32(b + 4, type);
    if (size >= 12) put32(b + 8, transaction);
    used += size; return b;
}
static void response(uint32_t transaction, uint16_t code)
{ uint8_t *b = packet(PTPIP_OPERATION_RESPONSE, 14, 0); word(b + 8, code); put32(b + 10, transaction); }
static void object(uint32_t transaction)
{
    uint8_t *b = packet(PTPIP_START_DATA, 20, transaction); put32(b + 12, 3);
    b = packet(PTPIP_DATA, 14, transaction); b[12] = 1; b[13] = 2;
    b = packet(PTPIP_END_DATA, 13, transaction); b[12] = 3;
    response(transaction, PTP_RC_OK);
}
static void reset(void)
{
    assert(!client.transaction_depth); used = at = sent_size = calls = 0; deadline = 0;
    client.last_result = PTPIP_CLIENT_OK;
}
static void reset_connection(void)
{
    reset(); /* mock tokens have no backend resources */
    client.channels[0].token = client.channels[1].token = 0;
    assert(ptpip_client_init(&client, APP_ENDPOINT_CAMERA, 9, NULL, NULL));
    client.channels[0].token = 42; client.channels[0].generation = 7;
    client.channels[1].token = 43; client.channels[1].generation = 7;
    atomic_store(&client.network_generation, 7); expected_token = 42;
}
static uint8_t *initial_ack(void)
{
    uint8_t *b = packet(PTPIP_INIT_COMMAND_ACK, 36, 55);
    b[12] = 0x42; b[28] = 'A'; put32(b + 32, PTPIP_PROTOCOL_VERSION); return b;
}
static ptp_data_status_t request(void)
{
    uint8_t output[16] = {0}; size_t size = 99; uint16_t code = 0;
    uint32_t expected = client.next_transaction;
    size_t sent_start = sent_size;
    ptp_data_status_t result = ptpip_client_request_data(&client, PTP_OC_GET_OBJECT, NULL, 0,
        output, sizeof(output), &size, &code);
    assert(client.next_transaction == expected + 1 && !client.transaction_depth && !client.transaction_deadline);
    assert(get32(sent + sent_start + 14) == expected);
    if (result == PTP_DATA_OK) assert(size == 3 && output[0] == 1 && output[1] == 2 && output[2] == 3);
    if (result == PTP_DATA_REFUSED) assert(!size && code == PTP_RC_ACCESS_DENIED && client.response == code);
    return result;
}
int main(void)
{
    assert(ptpip_client_init(&client, APP_ENDPOINT_CAMERA, 9, NULL, NULL));
    client.channels[0].token = 42; client.channels[0].generation = 7;
    atomic_store(&client.network_generation, 7);
    reset(); response(1, PTP_RC_ACCESS_DENIED); assert(request() == PTP_DATA_REFUSED);
    deadline = 0; object(2); assert(request() == PTP_DATA_OK && at == used);
    reset(); packet(PTPIP_PROBE_REQUEST, 8, 0); object(3);
    assert(request() == PTP_DATA_OK && get32(sent + 22) == PTPIP_PROBE_RESPONSE);
    reset(); response(99, PTP_RC_OK); assert(request() == PTP_DATA_PROTOCOL && client.last_result == PTPIP_CLIENT_PROTOCOL);
    reset(); uint8_t *b = packet(PTPIP_START_DATA, 20, client.next_transaction); put32(b + 12, 3);
    response(client.next_transaction, PTP_RC_ACCESS_DENIED); assert(request() == PTP_DATA_PROTOCOL);
    reset(); b = packet(PTPIP_START_DATA, 20, client.next_transaction); put32(b + 12, 3);
    packet(PTPIP_END_DATA, 14, client.next_transaction); assert(request() == PTP_DATA_PROTOCOL);
    reset(); b = packet(PTPIP_START_DATA, 20, client.next_transaction); put32(b + 12, 17); assert(request() == PTP_DATA_PROTOCOL);
    reset(); b = packet(PTPIP_START_DATA, 20, client.next_transaction); put32(b + 12, 3); put32(b + 16, 1); assert(request() == PTP_DATA_PROTOCOL);
    reset(); packet(PTPIP_OPERATION_RESPONSE, 15, client.next_transaction); assert(request() == PTP_DATA_PROTOCOL);
    reset(); object(client.next_transaction); --used; assert(request() == PTP_DATA_IO && client.last_result == PTPIP_CLIENT_NETWORK);
    reset(); packet(PTPIP_PROBE_REQUEST, 8, 0); response(client.next_transaction, PTP_RC_OK);
    assert(ptpip_client_operation(&client, PTP_OC_OPEN_SESSION, true) && client.session == 1);
    assert(get32(sent + 18) == 1 && get32(sent + 26) == PTPIP_PROBE_RESPONSE);
    reset(); response(client.next_transaction, PTP_RC_ACCESS_DENIED);
    assert(!ptpip_client_operation(&client, PTP_OC_CLOSE_SESSION, false) && client.session == 1 && client.response == PTP_RC_ACCESS_DENIED);
    reset(); response(client.next_transaction, PTP_RC_OK);
    assert(ptpip_client_operation(&client, PTP_OC_CLOSE_SESSION, false) && !client.session);
    for (unsigned width = 1; width <= 4; width *= 2) {
        reset(); uint32_t transaction = client.next_transaction;
        response(transaction, PTP_RC_OK); bool accepted = false; uint8_t value[4] = {0xff, 0xff, 0, 0};
        assert(ptpip_client_send_data(&client, 0x9010, 0x5010, value, width, &accepted) && accepted);
        assert(sent_size == 66 + width && !client.transaction_depth && client.next_transaction == transaction + 1);
        assert(get32(sent + 8) == PTPIP_DATA_PHASE_OUT && get32(sent + 14) == transaction);
        assert(get32(sent + 18) == 0x5010 && get32(sent + 34) == width);
        assert(get32(sent + 42) == 12 + width && get32(sent + 46) == PTPIP_DATA && get32(sent + 50) == transaction);
        assert(!memcmp(sent + 54, value, width));
        assert(get32(sent + 54 + width) == 12 && get32(sent + 58 + width) == PTPIP_END_DATA);
    }
    reset(); bool accepted = true; uint8_t value[2] = {2, 0};
    packet(PTPIP_PROBE_REQUEST, 8, 0); response(client.next_transaction, PTP_RC_ACCESS_DENIED);
    assert(ptpip_client_send_data(&client, 0x9010, 0x5010, value, 2, &accepted) && !accepted);
    assert(client.response == PTP_RC_ACCESS_DENIED && get32(sent + 72) == PTPIP_PROBE_RESPONSE);
    reset(); response(client.next_transaction + 1, PTP_RC_OK);
    assert(!ptpip_client_send_data(&client, 0x9010, 0x5010, value, 2, &accepted) && !accepted && client.last_result == PTPIP_CLIENT_PROTOCOL);
    reset(); uint32_t unchanged = client.next_transaction;
    assert(!ptpip_client_send_data(&client, 0x9010, 0x5010, NULL, 2, &accepted) && !calls);
    assert(!ptpip_client_send_data(&client, 0x9010, 0x5010, value, 117, &accepted) && !calls && client.next_transaction == unchanged);
    reset(); client.next_transaction = UINT32_MAX; uint8_t output[16]; size_t size; uint16_t code;
    assert(ptpip_client_request_data(&client, PTP_OC_GET_OBJECT, NULL, 0, output, sizeof(output), &size, &code) == PTP_DATA_PROTOCOL && !calls);
    reset_connection(); uint8_t guid[16] = {0x21};
    packet(PTPIP_PROBE_REQUEST, 8, 0); initial_ack(); int64_t started = now;
    assert(ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 120000));
    assert(deadline == started + 120000000 && !client.transaction_depth && client.command_initialized);
    assert(client.next_transaction == 2 && client.connection_id == 55 && client.peer_guid[0] == 0x42 && !strcmp(client.peer_name, "A"));
    assert(get32(sent) == 68 && get32(sent + 4) == PTPIP_INIT_COMMAND_REQUEST && !memcmp(sent + 8, guid, 16));
    assert(get16(sent + 24) == 'E' && get16(sent + 62) == 0 && get32(sent + 64) == PTPIP_PROTOCOL_VERSION);
    assert(get32(sent + 72) == PTPIP_PROBE_RESPONSE);
    unsigned previous_calls = calls;
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000) && calls == previous_calls);
    reset(); expected_token = 43; packet(PTPIP_INIT_EVENT_ACK, 8, 0);
    assert(ptpip_client_initialize_event(&client) && client.event_initialized && get32(sent + 8) == 55);
    assert(get32(sent) == 12 && get32(sent + 4) == PTPIP_INIT_EVENT_REQUEST);
    reset(); expected_token = 42; response(2, PTP_RC_OK);
    assert(ptpip_client_timeout_set(&client, PTPIP_CHANNEL_COMMAND, 5000));
    assert(ptpip_client_operation(&client, PTP_OC_OPEN_SESSION, true) && client.session == 1 && client.next_transaction == 3);
    reset_connection(); b = packet(PTPIP_INIT_FAIL, 12, 0x1234);
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000));
    assert(client.initialization_reason == 0x1234 && !client.command_initialized && client.last_result == PTPIP_CLIENT_INIT_REJECTED);
    reset_connection(); b = initial_ack(); put32(b + 32, 0x20000);
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000));
    reset_connection(); b = initial_ack(); b[30] = 1;
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000));
    reset_connection(); packet(PTPIP_INIT_COMMAND_ACK, 35, 55);
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000));
    reset_connection(); for (unsigned i = 0; i < 16; ++i) packet(PTPIP_PROBE_REQUEST, 8, 0);
    assert(!ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000) && !client.command_initialized);
    assert(sent_size == 68 + 16 * 8);
    reset_connection(); assert(!ptpip_client_initialize_event(&client) && !calls);
    initial_ack(); assert(ptpip_client_initialize_command(&client, guid, "ESP32-Camera-Remote", 10000));
    reset(); expected_token = 43; packet(PTPIP_INIT_EVENT_ACK, 12, 55);
    assert(!ptpip_client_initialize_event(&client) && !client.event_initialized && client.last_result == PTPIP_CLIENT_PROTOCOL);
    reset(); expected_token = 43; ptpip_event_t event; bool available;
    assert(ptpip_client_next_event(&client, &event, &available) && !available && !sent_size && !at);
    reset(); b = packet(PTPIP_EVENT, 26, 0); word(b + 8, 0xc203); put32(b + 10, 17);
    put32(b + 14, 1); put32(b + 18, 2); put32(b + 22, 3);
    assert(ptpip_client_next_event(&client, &event, &available) && available && !event.probe && event.code == 0xc203);
    assert(event.transaction == 17 && event.count == 3 && event.params[2] == 3 && !client.transaction_depth);
    reset(); packet(PTPIP_PROBE_REQUEST, 8, 0);
    assert(ptpip_client_next_event(&client, &event, &available) && available && event.probe && get32(sent + 4) == PTPIP_PROBE_RESPONSE);
    reset(); packet(PTPIP_EVENT, 15, 0);
    assert(!ptpip_client_next_event(&client, &event, &available) && !available && client.last_result == PTPIP_CLIENT_PROTOCOL);
    reset(); expected_token = 42; uint32_t first_sony = client.next_transaction;
    response(client.next_transaction, PTP_RC_OK);
    assert(sony_encode_set_exposure_mode(&writer, 0x10002, &accepted) && accepted);
    assert(get16(sent + 12) == SONY_OC_SET_CONTROL_DEVICE_A && get32(sent + 18) == SONY_DPC_EXPOSURE_PROGRAM);
    assert(get32(sent + 34) == 4 && get32(sent + 54) == 0x10002 && get32(sent + 14) == first_sony);
    for (int direction = -1; direction <= 1; direction += 2) {
        reset(); uint32_t transaction = client.next_transaction; response(transaction, PTP_RC_OK);
        assert(sony_encode_manual_focus_step(&writer, direction, &accepted) && accepted);
        assert(get32(sent + 18) == SONY_DPC_MANUAL_FOCUS_ADJUST && get16(sent + 54) == (uint16_t)(int16_t)direction);
        assert(get32(sent + 14) == transaction && client.next_transaction == transaction + 1);
    }
    for (int direction = -1; direction <= 1; ++direction) {
        reset(); response(client.next_transaction, PTP_RC_OK);
        assert(sony_encode_zoom(&writer, direction, &accepted) && accepted && sent[54] == (uint8_t)(int8_t)direction);
        assert(get32(sent + 18) == SONY_DPC_ZOOM_OPERATION && get32(sent + 34) == 1);
    }
    for (unsigned full = 0; full < 2; ++full) for (unsigned pressed = 0; pressed < 2; ++pressed) {
        reset(); response(client.next_transaction, PTP_RC_OK);
        assert(sony_encode_shutter_button(&writer, full, pressed, &accepted) && accepted);
        assert(get32(sent + 18) == (full ? SONY_DPC_SHUTTER_RELEASE : SONY_DPC_SHUTTER_HALF_RELEASE));
        assert(get16(sent + 54) == (pressed ? 2 : 1));
        reset(); response(client.next_transaction, PTP_RC_OK);
        assert(sony_encode_movie_record(&writer, pressed, &accepted) && accepted);
        assert(get32(sent + 18) == SONY_DPC_MOVIE_RECORD && get16(sent + 54) == (pressed ? 2 : 1));
    }
    const uint16_t properties[2] = {SONY_DPC_SHUTTER_SPEED, SONY_DPC_F_NUMBER};
    for (unsigned i = 0; i < 2; ++i) for (int direction = -1; direction <= 1; direction += 2) {
        reset(); response(client.next_transaction, PTP_RC_OK);
        assert(sony_encode_setting_step(&writer, properties[i], direction, &accepted) && accepted);
        assert(get32(sent + 18) == properties[i] && sent[54] == (uint8_t)(int8_t)direction);
    }
    reset(); packet(PTPIP_PROBE_REQUEST, 8, 0); response(client.next_transaction, PTP_RC_ACCESS_DENIED);
    assert(sony_encode_set_scalar(&writer, SONY_DPC_EXPOSURE_PROGRAM, 6, 1, &accepted) && !accepted);
    assert(client.response == PTP_RC_ACCESS_DENIED && get32(sent + 74) == PTPIP_PROBE_RESPONSE);
    reset(); uint32_t before_invalid = client.next_transaction;
    assert(!sony_encode_manual_focus_step(&writer, 0, &accepted));
    assert(!sony_encode_zoom(&writer, 2, &accepted));
    assert(!sony_encode_setting_step(&writer, SONY_DPC_ISO, 1, &accepted));
    assert(!sony_encode_set_scalar(&writer, SONY_DPC_ISO, 2, 256, &accepted));
    writer.context=NULL;
    assert(!sony_encode_set_scalar(&writer, SONY_DPC_ISO, 6, 1, &accepted));
    assert(!calls && !accepted && client.next_transaction == before_invalid);
    return 0;
}
