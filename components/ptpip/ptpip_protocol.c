#include "ptpip_protocol.h"
#include "private/ptp_wire.h"
#include "ptp_codes.h"
#include "ptpip_packet.h"
#include <string.h>
typedef struct { ptpip_client_t *client; ptpip_channel_kind_t kind; } stream_t;
static bool transfer(void *context, void *buffer, size_t size, bool transmit)
{ stream_t *stream = context; return ptpip_client_transfer(stream->client, stream->kind, buffer, size, transmit); }
static bool begin(void *context)
{ stream_t *stream = context; return ptpip_client_transaction_begin(stream->client, stream->kind); }
static void end(void *context)
{ stream_t *stream = context; ptpip_client_transaction_end(stream->client); }
static ptp_wire_io_t io(stream_t *stream)
{ return (ptp_wire_io_t){stream, transfer, begin, end}; }
static bool valid(ptpip_client_t *client)
{ return client && client->api_version == PTPIP_CLIENT_API_VERSION; }
static bool next(ptpip_client_t *client, uint32_t *transaction)
{
    if (!valid(client) || !client->next_transaction || client->next_transaction == UINT32_MAX) return false;
    *transaction = client->next_transaction++;
    return true;
}
bool ptpip_client_initialize_command(ptpip_client_t *client, const uint8_t guid[16],
    const char *name, unsigned timeout_ms)
{
    if (!valid(client) || !guid || !name || !timeout_ms || !client->channels[PTPIP_CHANNEL_COMMAND].token ||
        client->command_initialized || client->session) return false;
    size_t name_size = strlen(name) + 1;
    if (name_size > (512 - 28) / 2) return false;
    uint8_t packet[512] = {0};
    uint32_t length = 24 + name_size * 2 + 4;
    put32(packet, length); put32(packet + 4, PTPIP_INIT_COMMAND_REQUEST);
    memcpy(packet + 8, guid, 16);
    for (size_t i = 0; i < name_size; ++i) packet[24 + i * 2] = name[i];
    put32(packet + length - 4, PTPIP_PROTOCOL_VERSION);
    ptpip_client_timeout_set(client, PTPIP_CHANNEL_COMMAND, timeout_ms);
    client->initialization_reason = 0;
    stream_t stream = {client, PTPIP_CHANNEL_COMMAND}; ptp_wire_io_t wire = io(&stream);
    int n = ptp_wire_initialization_exchange(&wire, packet, length, sizeof(packet), NULL);
    if (n >= 12 && get32(packet + 4) == PTPIP_INIT_FAIL) {
        client->initialization_reason = get32(packet + 8);
        client->last_result = PTPIP_CLIENT_INIT_REJECTED; return false;
    }
    if (n < 34 || (n - 32) % 2 || get32(packet + 4) != PTPIP_INIT_COMMAND_ACK ||
        get16(packet + n - 6) != 0 || get32(packet + n - 4) != PTPIP_PROTOCOL_VERSION) {
        if (client->last_result == PTPIP_CLIENT_OK) client->last_result = PTPIP_CLIENT_PROTOCOL;
        return false;
    }
    client->connection_id = get32(packet + 8);
    memcpy(client->peer_guid, packet + 12, sizeof(client->peer_guid));
    memset(client->peer_name, 0, sizeof(client->peer_name));
    for (size_t i = 0; i < sizeof(client->peer_name) - 1 && 28 + i * 2 + 1 < (size_t)n - 4; ++i) {
        client->peer_name[i] = packet[28 + i * 2];
        if (!client->peer_name[i]) break;
    }
    client->command_initialized = true;
    client->next_transaction = 2;
    return true;
}
bool ptpip_client_initialize_event(ptpip_client_t *client)
{
    if (!valid(client) || !client->command_initialized || client->event_initialized ||
        !client->channels[PTPIP_CHANNEL_EVENT].token) return false;
    uint8_t packet[512] = {0};
    put32(packet, 12); put32(packet + 4, PTPIP_INIT_EVENT_REQUEST); put32(packet + 8, client->connection_id);
    stream_t stream = {client, PTPIP_CHANNEL_EVENT}; ptp_wire_io_t wire = io(&stream);
    int n = ptp_wire_initialization_exchange(&wire, packet, 12, sizeof(packet), NULL);
    bool ok = n == 8 && get32(packet + 4) == PTPIP_INIT_EVENT_ACK;
    if (ok) client->event_initialized = true;
    else if (client->last_result == PTPIP_CLIENT_OK) client->last_result = PTPIP_CLIENT_PROTOCOL;
    return ok;
}
bool ptpip_client_next_event(ptpip_client_t *client, ptpip_event_t *event, bool *available)
{
    if (available) *available = false;
    if (!valid(client) || !event || !available) return false;
    *event = (ptpip_event_t){0}; bool readable;
    if (!ptpip_client_poll(client, PTPIP_CHANNEL_EVENT, &readable)) return false;
    if (!readable) return true;
    if (!ptpip_client_transaction_begin(client, PTPIP_CHANNEL_EVENT)) return false;
    uint8_t packet[512];
    int n = ptpip_client_receive_packet(client, PTPIP_CHANNEL_EVENT, packet, sizeof(packet));
    bool ok = false;
    if (n == 8 && get32(packet + 4) == PTPIP_PROBE_REQUEST) {
        put32(packet + 4, PTPIP_PROBE_RESPONSE);
        ok = ptpip_client_transfer(client, PTPIP_CHANNEL_EVENT, packet, 8, true);
        event->probe = true;
    } else if (n >= 0 && ptp_wire_decode_event(packet, (size_t)n, event)) {
        ok = true;
    } else if (client->last_result == PTPIP_CLIENT_OK) client->last_result = PTPIP_CLIENT_PROTOCOL;
    ptpip_client_transaction_end(client);
    *available = ok; return ok;
}
int ptpip_client_receive_packet(ptpip_client_t *client, ptpip_channel_kind_t kind,
    uint8_t *packet, size_t capacity)
{
    if (!valid(client) || (unsigned)kind >= PTPIP_CHANNEL_COUNT) return -1;
    stream_t stream = {client, kind}; ptp_wire_io_t wire = io(&stream);
    int result = ptp_wire_receive_packet(&wire, packet, capacity);
    if (result < 0 && client->last_result == PTPIP_CLIENT_OK) client->last_result = PTPIP_CLIENT_PROTOCOL;
    return result;
}
bool ptpip_client_send_data(ptpip_client_t *client, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted)
{
    if (accepted) *accepted = false;
    uint32_t transaction;
    if (!accepted || !data || size > 116 || !next(client, &transaction)) return false;
    stream_t stream = {client, PTPIP_CHANNEL_COMMAND}; ptp_wire_io_t wire = io(&stream);
    bool result = ptp_wire_send_data(&wire, opcode, transaction, property, data, size,
        &client->response, NULL);
    *accepted = result && client->response == PTP_RC_OK;
    if (!result && client->last_result == PTPIP_CLIENT_OK) client->last_result = PTPIP_CLIENT_PROTOCOL;
    return result;
}
bool ptpip_client_operation(ptpip_client_t *client, uint16_t opcode, bool session_param)
{
    uint32_t transaction;
    if (!next(client, &transaction)) return false;
    stream_t stream = {client, PTPIP_CHANNEL_COMMAND}; ptp_wire_io_t wire = io(&stream);
    bool result = ptp_wire_operation(&wire, opcode, transaction, session_param, &client->response);
    if (!result && !client->response && client->last_result == PTPIP_CLIENT_OK)
        client->last_result = PTPIP_CLIENT_PROTOCOL;
    if (result && opcode == PTP_OC_OPEN_SESSION) client->session = 1;
    if (result && opcode == PTP_OC_CLOSE_SESSION) client->session = 0;
    return result;
}
ptp_data_status_t ptpip_client_request_data(ptpip_client_t *client, uint16_t opcode,
    const uint32_t *params, unsigned count, uint8_t *output, size_t capacity,
    size_t *size, uint16_t *response)
{
    if (size) *size = 0;
    if (response) *response = 0;
    uint32_t transaction;
    if (!output || !size || !response || (count && !params) || count > 5 || !next(client, &transaction))
        return PTP_DATA_PROTOCOL;
    stream_t stream = {client, PTPIP_CHANNEL_COMMAND}; ptp_wire_io_t wire = io(&stream);
    ptp_data_status_t result = ptp_wire_request_data_result(&wire, opcode, transaction,
        params, count, output, capacity, size, response);
    client->response = *response;
    if (result == PTP_DATA_PROTOCOL) client->last_result = PTPIP_CLIENT_PROTOCOL;
    return result;
}
