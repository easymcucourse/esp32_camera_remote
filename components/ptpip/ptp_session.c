#include "ptp_codes.h"
#include "ptp_session.h"
#include "ptpip_packet.h"
#include "ptpip_transport.h"
#include "lwip/sockets.h"
#include <errno.h>
#include "esp_log.h"
static const char *TAG = "camera_pair";

// The diagnostic expects small handshake/response packets only, never image data.
static int receive_packet(int fd, uint8_t *packet, size_t capacity)
{
    if (!ptpip_transfer(fd, packet, 8, false)) return -1;
    uint32_t length = get32(packet);
    if (length < 8 || length > capacity) {
        ESP_LOGE(TAG, "Unexpected packet length=%lu", (unsigned long)length);
        return -1;
    }
    if (!ptpip_transfer(fd, packet + 8, length - 8, false)) return -1;
    ESP_LOGI(TAG, "RX type=%lu length=%lu", (unsigned long)get32(packet + 4), (unsigned long)length);
    return (int)length;
}

int ptp_receive_packet(int fd, uint8_t *packet, size_t capacity)
{
    if (!packet || capacity < 8 || !ptpip_transaction_begin(fd)) return -1;
    int result = receive_packet(fd, packet, capacity);
    ptpip_transaction_end(); return result;
}

static bool operation(int fd, uint16_t code, uint32_t transaction, bool session_param)
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
    if (!ptpip_transfer(fd, packet, length, true)) return false;
    for (unsigned i = 0; i < 16; ++i) {
        int n = ptp_receive_packet(fd, packet, sizeof(packet));
        if (n == 8 && get32(packet + 4) == PTPIP_PROBE_REQUEST) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!ptpip_transfer(fd, packet, 8, true)) return false;
            continue;
        }
        if (n < 14 || n > 34 || (n - 14) % 4 ||
            get32(packet + 4) != PTPIP_OPERATION_RESPONSE || get32(packet + 10) != transaction) return false;
        uint16_t response = get16(packet + 8);
        ESP_LOGI(TAG, "Operation 0x%04x response=0x%04x transaction=%lu", code, response, (unsigned long)transaction);
        return response == PTP_RC_OK;
    }
    return false;
}

bool ptp_operation(int fd, uint16_t code, uint32_t transaction, bool session_param)
{
    if (!ptpip_transaction_begin(fd)) return false;
    bool result = operation(fd, code, transaction, session_param);
    ptpip_transaction_end(); return result;
}

static ptp_data_status_t request_data_result(int fd, uint16_t opcode, uint32_t transaction,
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
    if (!ptpip_transfer(fd, packet, length, true)) return PTP_DATA_IO;
    size_t expected = 0;
    bool started = false, ended = false;
    for (unsigned packets = 0; packets < 256; ++packets) {
        if (!ptpip_transfer(fd, packet, 8, false)) return PTP_DATA_IO;
        length = get32(packet);
        uint32_t type = get32(packet + 4);
        if (length < 8) return PTP_DATA_PROTOCOL;
        if (type == PTPIP_PROBE_REQUEST && length == 8) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!ptpip_transfer(fd, packet, 8, true)) return PTP_DATA_IO;
        } else if (type == PTPIP_START_DATA && length == 20 && !started) {
            if (!ptpip_transfer(fd, packet + 8, 12, false)) return PTP_DATA_IO;
            if (get32(packet + 8) != transaction || get32(packet + 16) ||
                get32(packet + 12) > capacity) return PTP_DATA_PROTOCOL;
            expected = get32(packet + 12); started = true;
        } else if ((type == PTPIP_DATA || type == PTPIP_END_DATA) &&
                   length >= 12 && started && !ended) {
            if (!ptpip_transfer(fd, packet + 8, 4, false)) return PTP_DATA_IO;
            size_t payload_size = length - 12;
            if (get32(packet + 8) != transaction || payload_size > expected - *output_size)
                return PTP_DATA_PROTOCOL;
            if (!ptpip_transfer(fd, output + *output_size, payload_size, false)) return PTP_DATA_IO;
            *output_size += payload_size;
            if (type == PTPIP_END_DATA) {
                ended = true;
                if (*output_size != expected) return PTP_DATA_PROTOCOL;
            }
        } else if (type == PTPIP_OPERATION_RESPONSE && length >= 14 && length <= 34 && !((length - 14) % 4)) {
            if (!ptpip_transfer(fd, packet + 8, length - 8, false)) return PTP_DATA_IO;
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

ptp_data_status_t ptp_request_data_result(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity,
    size_t *output_size, uint16_t *response)
{
    if (output_size) *output_size = 0;
    if (response) *response = 0;
    if (!ptpip_transaction_begin(fd)) return PTP_DATA_IO;
    ptp_data_status_t result = request_data_result(fd, opcode, transaction, params,
        num_params, output, capacity, output_size, response);
    ptpip_transaction_end(); return result;
}

bool ptp_request_data(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity, size_t *output_size)
{
    uint16_t response;
    return ptp_request_data_result(fd, opcode, transaction, params, num_params,
                                  output, capacity, output_size, &response) == PTP_DATA_OK;
}

bool ptp_drain_events_changed(int event, bool *properties_changed)
{
    if (!properties_changed) return false;
    *properties_changed = false;
    // Drain events between frames so the independent event connection cannot fill.
    for (unsigned i = 0; i < 32; ++i) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(event, &readable);
        struct timeval poll = {0};
        int ready = select(event + 1, &readable, NULL, NULL, &poll);
        if (ready == 0) return true;
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return false;
        uint8_t packet[512];
        int length = ptp_receive_packet(event, packet, sizeof(packet));
        if (length < 8) return false;
        uint32_t type = get32(packet + 4);
        if (type == PTPIP_PROBE_REQUEST && length == 8) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!ptpip_transfer(event, packet, 8, true)) return false;
        } else if (type != PTPIP_EVENT || length < 14 || length > 26 || (length - 14) % 4) {
            ESP_LOGW(TAG, "Unexpected event type=%lu", (unsigned long)type);
            return false;
        } else if (get16(packet + 8) == 0xc203) {
            // Captured param=0 identifies no specific property: refresh all.
            *properties_changed = true;
        }
    }
    return true;
}


bool ptp_drain_events(int event)
{
    bool changed;
    return ptp_drain_events_changed(event, &changed);
}
