#include "sony_codes.h"
#include "ptp_codes.h"
#include "sony_ext.h"
#include "ptpip_packet.h"
#include "ptpip_transport.h"
#include "ptp_session.h"
#include <string.h>
#include "esp_log.h"
static const char *TAG = "camera_pair";

// Sony SetExtDevicePropValue (0x9205), UINT32 ExposureProgram (0x500e).
// Called exclusively by the socket owner between live-view transactions.
static bool write_value(int fd, uint32_t transaction, uint16_t opcode,
                             uint16_t property, const uint8_t *data, size_t size,
                             bool *accepted)
{
    uint8_t packet[128] = {0};
    *accepted = false;
    if (!data || size > sizeof(packet) - 12) return false;
    put32(packet, 22); put32(packet + 4, PTPIP_OPERATION_REQUEST); put32(packet + 8, PTPIP_DATA_PHASE_OUT);
    packet[12] = opcode & 0xff;
    packet[13] = opcode >> 8;
    put32(packet + 14, transaction); put32(packet + 18, property);
    if (!ptpip_transfer(fd, packet, 22, true)) return false;
    memset(packet, 0, 20);
    put32(packet, 20); put32(packet + 4, PTPIP_START_DATA); put32(packet + 8, transaction);
    put32(packet + 12, size);
    if (!ptpip_transfer(fd, packet, 20, true)) return false;
    put32(packet, 12 + size); put32(packet + 4, PTPIP_DATA); put32(packet + 8, transaction);
    memcpy(packet + 12, data, size);
    if (!ptpip_transfer(fd, packet, 12 + size, true)) return false;
    put32(packet, 12); put32(packet + 4, PTPIP_END_DATA); put32(packet + 8, transaction);
    if (!ptpip_transfer(fd, packet, 12, true)) return false;
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
        *accepted = response == PTP_RC_OK;
        ESP_LOGI(TAG, "Write opcode=0x%04x property=0x%04x transaction=%lu bytes=%u response=0x%04x",
                 opcode, property, (unsigned long)transaction, (unsigned)size, response);
        return true; // A camera rejection does not invalidate the TCP session.
    }
    return false;
}

static bool sony_write_value(int fd, uint32_t transaction, uint16_t opcode,
                             uint16_t property, const uint8_t *data, size_t size, bool *accepted)
{
    if (accepted) *accepted = false;
    if (!ptpip_transaction_begin(fd)) return false;
    bool result = write_value(fd, transaction, opcode, property, data, size, accepted);
    ptpip_transaction_end(); return result;
}

bool sony_set_exposure_mode(int fd, uint32_t transaction, uint32_t value, bool *accepted)
{
    uint8_t data[4];
    put32(data, value);
    return sony_write_value(fd, transaction, SONY_OC_SET_CONTROL_DEVICE_A,
                            SONY_DPC_EXPOSURE_PROGRAM, data, sizeof(data), accepted);
}

bool sony_manual_focus_step(int fd, uint32_t transaction, int direction, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    if (direction != 1 && direction != -1) return false;
    uint16_t value = (uint16_t)(int16_t)direction;
    uint8_t data[2] = {(uint8_t)value, (uint8_t)(value >> 8)};
    return sony_write_value(fd, transaction, SONY_OC_SET_CONTROL_DEVICE_B,
                            SONY_DPC_MANUAL_FOCUS_ADJUST, data, sizeof(data), accepted);
}

