#include "ptp_codes.h"
#include "private/ptp_wire.h"
#include "ptpip_packet.h"
#include "esp_log.h"
#include <string.h>
static const char *TAG = "camera_pair";
bool ptp_wire_decode_event(const uint8_t *packet, size_t size, ptpip_event_t *event)
{
    if (!packet || !event || size < 14 || size > 26 || (size - 14) % 4 ||
        get32(packet) != size || get32(packet + 4) != PTPIP_EVENT) return false;
    *event = (ptpip_event_t){.code = get16(packet + 8), .transaction = get32(packet + 10),
        .count = (unsigned)(size - 14) / 4};
    for (unsigned i = 0; i < event->count; ++i) event->params[i] = get32(packet + 14 + i * 4);
    return true;
}

int ptp_wire_initialization_exchange(const ptp_wire_io_t *io, uint8_t *packet,
    size_t request_length, size_t capacity, ptp_wire_packet_fn receive)
{
    if (!packet || request_length < 8 || request_length > capacity || !io->begin(io->context)) return -1;
    int result = -1;
    if (io->transfer(io->context, packet, request_length, true)) {
        for (unsigned i = 0; i < 16; ++i) {
            result = receive ? receive(io, packet, capacity) : ptp_wire_receive_packet(io, packet, capacity);
            if (result != 8 || get32(packet + 4) != PTPIP_PROBE_REQUEST) break;
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!io->transfer(io->context, packet, 8, true)) { result = -1; break; }
            result = -1;
        }
    }
    io->end(io->context); return result;
}

// The diagnostic expects small handshake/response packets only, never image data.
static int receive_packet(const ptp_wire_io_t *io, uint8_t *packet, size_t capacity)
{
    if (!io->transfer(io->context, packet, 8, false)) return -1;
    uint32_t length = get32(packet);
    if (length < 8 || length > capacity) {
        ESP_LOGE(TAG, "Unexpected packet length=%lu", (unsigned long)length);
        return -1;
    }
    if (!io->transfer(io->context, packet + 8, length - 8, false)) return -1;
    ESP_LOGI(TAG, "RX type=%lu length=%lu", (unsigned long)get32(packet + 4), (unsigned long)length);
    return (int)length;
}

int ptp_wire_receive_packet(const ptp_wire_io_t *io, uint8_t *packet, size_t capacity)
{
    if (!packet || capacity < 8 || !io->begin(io->context)) return -1;
    int result = receive_packet(io, packet, capacity);
    io->end(io->context); return result;
}

static bool operation(const ptp_wire_io_t *io, uint16_t code, uint32_t transaction, bool session_param, uint16_t *response_code)
{
    uint8_t packet[128] = {0};
    uint32_t length = session_param ? 22 : 18;
    put32(packet, length);
    put32(packet + 4, PTPIP_OPERATION_REQUEST);
    put32(packet + 8, PTPIP_DATA_PHASE_NONE_OR_IN);
    packet[12] = code;
    packet[13] = code >> 8;
    put32(packet + 14, transaction);
    if (session_param) put32(packet + 18, 1);
    if (!io->transfer(io->context, packet, length, true)) return false;
    for (unsigned i = 0; i < 16; ++i) {
        int n = ptp_wire_receive_packet(io, packet, sizeof(packet));
        if (n == 8 && get32(packet + 4) == PTPIP_PROBE_REQUEST) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!io->transfer(io->context, packet, 8, true)) return false;
            continue;
        }
        if (n < 14 || n > 34 || (n - 14) % 4 ||
            get32(packet + 4) != PTPIP_OPERATION_RESPONSE || get32(packet + 10) != transaction) return false;
        uint16_t response = get16(packet + 8);
        if (response_code) *response_code = response;
        ESP_LOGI(TAG, "Operation 0x%04x response=0x%04x transaction=%lu", code, response, (unsigned long)transaction);
        return response == PTP_RC_OK;
    }
    return false;
}

bool ptp_wire_operation(const ptp_wire_io_t *io, uint16_t code, uint32_t transaction, bool session_param, uint16_t *response)
{
    if (response) *response = 0;
    if (!io->begin(io->context)) return false;
    bool result = operation(io, code, transaction, session_param, response);
    io->end(io->context); return result;
}

static ptp_data_status_t request_data_result(const ptp_wire_io_t *io, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity,
    size_t *output_size, uint16_t *response)
{
    uint8_t packet[64] = {0};
    if (!output_size || !response || !output || (num_params && !params) || num_params > 5)
        return PTP_DATA_PROTOCOL;
    *output_size = 0; *response = 0;
    uint32_t length = 18 + num_params * 4;
    put32(packet, length); put32(packet + 4, PTPIP_OPERATION_REQUEST);
    put32(packet + 8, PTPIP_DATA_PHASE_NONE_OR_IN);
    packet[12] = opcode; packet[13] = opcode >> 8;
    put32(packet + 14, transaction);
    for (unsigned i = 0; i < num_params; ++i) put32(packet + 18 + i * 4, params[i]);
    if (!io->transfer(io->context, packet, length, true)) return PTP_DATA_IO;
    size_t expected = 0;
    bool started = false, ended = false;
    for (unsigned packets = 0; packets < 256; ++packets) {
        if (!io->transfer(io->context, packet, 8, false)) return PTP_DATA_IO;
        length = get32(packet);
        uint32_t type = get32(packet + 4);
        if (length < 8) return PTP_DATA_PROTOCOL;
        if (type == PTPIP_PROBE_REQUEST && length == 8) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!io->transfer(io->context, packet, 8, true)) return PTP_DATA_IO;
        } else if (type == PTPIP_START_DATA && length == 20 && !started) {
            if (!io->transfer(io->context, packet + 8, 12, false)) return PTP_DATA_IO;
            if (get32(packet + 8) != transaction || get32(packet + 16) ||
                get32(packet + 12) > capacity) return PTP_DATA_PROTOCOL;
            expected = get32(packet + 12); started = true;
        } else if ((type == PTPIP_DATA || type == PTPIP_END_DATA) &&
                   length >= 12 && started && !ended) {
            if (!io->transfer(io->context, packet + 8, 4, false)) return PTP_DATA_IO;
            size_t payload_size = length - 12;
            if (get32(packet + 8) != transaction || payload_size > expected - *output_size)
                return PTP_DATA_PROTOCOL;
            if (!io->transfer(io->context, output + *output_size, payload_size, false)) return PTP_DATA_IO;
            *output_size += payload_size;
            if (type == PTPIP_END_DATA) {
                ended = true;
                if (*output_size != expected) return PTP_DATA_PROTOCOL;
            }
        } else if (type == PTPIP_OPERATION_RESPONSE && length >= 14 && length <= 34 && !((length - 14) % 4)) {
            if (!io->transfer(io->context, packet + 8, length - 8, false)) return PTP_DATA_IO;
            if (get32(packet + 10) != transaction || (started && !ended)) return PTP_DATA_PROTOCOL;
            *response = get16(packet + 8);
            if (*response != PTP_RC_OK)
                ESP_LOGW(TAG, "Read opcode=0x%04x transaction=%lu response=0x%04x bytes=%u",
                         opcode, (unsigned long)transaction, *response, (unsigned)*output_size);
            return *response == PTP_RC_OK ? PTP_DATA_OK : PTP_DATA_REFUSED;
        } else {
            ESP_LOGE(TAG, "Protocol error opcode=0x%04x transaction=%lu type=%lu length=%lu",
                     opcode, (unsigned long)transaction, (unsigned long)type, (unsigned long)length);
            return PTP_DATA_PROTOCOL;
        }
    }
    return PTP_DATA_PROTOCOL;
}

ptp_data_status_t ptp_wire_request_data_result(const ptp_wire_io_t *io, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity,
    size_t *output_size, uint16_t *response)
{
    if (output_size) *output_size = 0;
    if (response) *response = 0;
    if (!io->begin(io->context)) return PTP_DATA_IO;
    ptp_data_status_t result = request_data_result(io, opcode, transaction, params,
        num_params, output, capacity, output_size, response);
    io->end(io->context); return result;
}


static bool send_data(const ptp_wire_io_t *io, uint32_t transaction, uint16_t opcode,
                             uint16_t property, const uint8_t *data, size_t size,
                             uint16_t *response_code, ptp_wire_packet_fn receive)
{
    uint8_t packet[128] = {0};
    *response_code = 0;
    if (!data || size > sizeof(packet) - 12) return false;
    put32(packet, 22); put32(packet + 4, PTPIP_OPERATION_REQUEST); put32(packet + 8, PTPIP_DATA_PHASE_OUT);
    packet[12] = opcode & 0xff;
    packet[13] = opcode >> 8;
    put32(packet + 14, transaction); put32(packet + 18, property);
    if (!io->transfer(io->context, packet, 22, true)) return false;
    memset(packet, 0, 20);
    put32(packet, 20); put32(packet + 4, PTPIP_START_DATA); put32(packet + 8, transaction);
    put32(packet + 12, size);
    if (!io->transfer(io->context, packet, 20, true)) return false;
    put32(packet, 12 + size); put32(packet + 4, PTPIP_DATA); put32(packet + 8, transaction);
    memcpy(packet + 12, data, size);
    if (!io->transfer(io->context, packet, 12 + size, true)) return false;
    put32(packet, 12); put32(packet + 4, PTPIP_END_DATA); put32(packet + 8, transaction);
    if (!io->transfer(io->context, packet, 12, true)) return false;
    for (unsigned i = 0; i < 16; ++i) {
        int n = receive ? receive(io, packet, sizeof(packet)) : ptp_wire_receive_packet(io, packet, sizeof(packet));
        if (n == 8 && get32(packet + 4) == PTPIP_PROBE_REQUEST) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!io->transfer(io->context, packet, 8, true)) return false;
            continue;
        }
        if (n < 14 || n > 34 || (n - 14) % 4 ||
            get32(packet + 4) != PTPIP_OPERATION_RESPONSE || get32(packet + 10) != transaction) return false;
        uint16_t response = get16(packet + 8);
        *response_code = response;
        ESP_LOGI(TAG, "Write opcode=0x%04x property=0x%04x transaction=%lu bytes=%u response=0x%04x",
                 opcode, property, (unsigned long)transaction, (unsigned)size, response);
        return true; // A camera rejection does not invalidate the TCP session.
    }
    return false;
}


bool ptp_wire_send_data(const ptp_wire_io_t *io, uint16_t opcode, uint32_t transaction,
    uint16_t property, const uint8_t *data, size_t size, uint16_t *response, ptp_wire_packet_fn receive)
{
    if (!response) return false;
    *response = 0;
    if (!io->begin(io->context)) return false;
    bool result = send_data(io, transaction, opcode, property, data, size, response, receive);
    io->end(io->context); return result;
}
