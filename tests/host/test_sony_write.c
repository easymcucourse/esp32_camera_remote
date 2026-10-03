#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sony_ext.h"
#include "ptpip_packet.h"
#include "ptp_codes.h"
#include "sony_codes.h"

static unsigned depth;
bool ptpip_transaction_begin(int fd) { (void)fd; ++depth; return true; }
void ptpip_transaction_end(void) { assert(depth); --depth; }
static uint8_t sent[256];
static size_t used;
static uint16_t response = PTP_RC_OK;
static uint32_t reply_tid = 42;
static bool probe;
bool ptpip_transfer(int fd, void *data, size_t length, bool transmit)
{
    (void)fd;
    assert(transmit && used + length <= sizeof(sent));
    memcpy(sent + used, data, length); used += length;
    return true;
}
int ptp_receive_packet(int fd, uint8_t *packet, size_t capacity)
{
    (void)fd; assert(capacity >= 14);
    memset(packet, 0, 14);
    if (probe) {
        probe = false; put32(packet, 8); put32(packet + 4, PTPIP_PROBE_REQUEST); return 8;
    }
    put32(packet, 14); put32(packet + 4, PTPIP_OPERATION_RESPONSE);
    packet[8] = (uint8_t)response; packet[9] = response >> 8;
    put32(packet + 10, reply_tid);
    return 14;
}
static void check(int direction, uint16_t value)
{
    used = 0; bool accepted = false;
    assert(sony_manual_focus_step(0, 42, direction, &accepted) && accepted);
    assert(used == 68 && depth == 0);
    assert(get32(sent) == 22 && get32(sent + 4) == PTPIP_OPERATION_REQUEST);
    assert(get32(sent + 8) == PTPIP_DATA_PHASE_OUT && get16(sent + 12) == SONY_OC_SET_CONTROL_DEVICE_B);
    assert(get32(sent + 14) == 42 && get32(sent + 18) == SONY_DPC_MANUAL_FOCUS_ADJUST);
    assert(get32(sent + 22) == 20 && get32(sent + 26) == PTPIP_START_DATA);
    assert(get32(sent + 30) == 42 && get32(sent + 34) == 2 && get32(sent + 38) == 0);
    assert(get32(sent + 42) == 14 && get32(sent + 46) == PTPIP_DATA);
    assert(get32(sent + 50) == 42 && get16(sent + 54) == value);
    assert(get32(sent + 56) == 12 && get32(sent + 60) == PTPIP_END_DATA && get32(sent + 64) == 42);
}
int main(void)
{
    check(1, 1); check(-1, 0xffff);
    bool accepted;
    used = 0; response = 0x200f;
    assert(sony_manual_focus_step(0, 42, 1, &accepted) && !accepted);
    response = PTP_RC_OK; reply_tid = 43; used = 0;
    assert(!sony_manual_focus_step(0, 42, 1, &accepted));
    reply_tid = 42; used = 0; probe = true;
    assert(sony_manual_focus_step(0, 42, 1, &accepted) && accepted);
    assert(used == 76 && get32(sent + 72) == PTPIP_PROBE_RESPONSE);
    used = 0;
    assert(!sony_manual_focus_step(0, 42, 0, &accepted) && used == 0);
    assert(sony_set_exposure_mode(0, 42, 0x10002, &accepted) && accepted);
    assert(used == 70 && get16(sent + 12) == SONY_OC_SET_CONTROL_DEVICE_A);
    assert(get32(sent + 18) == SONY_DPC_EXPOSURE_PROGRAM && get32(sent + 34) == 4);
    assert(get32(sent + 42) == 16 && get32(sent + 54) == 0x10002);
    used = 0;
    assert(sony_set_scalar(0, 42, SONY_DPC_FOCUS_MODE, 4, 1, &accepted) && accepted);
    assert(used == 68 && get32(sent + 34) == 2 && get16(sent + 54) == 1);
    used = 0;
    assert(sony_set_scalar(0, 42, SONY_DPC_ZOOM_ENABLE_STATUS, 2, 1, &accepted) && accepted);
    assert(used == 67 && get32(sent + 34) == 1 && sent[54] == 1);
    used = 0;
    assert(sony_set_scalar(0, 42, SONY_DPC_EXPOSURE_BIAS, 3, 0xfff0, &accepted) && accepted);
    assert(get16(sent + 54) == 0xfff0);
    used = 0;
    assert(!sony_set_scalar(0, 42, SONY_DPC_FOCUS_MODE, 4, 0x10000, &accepted));
    assert(!sony_set_scalar(0, 42, SONY_DPC_FOCUS_MODE, 8, 1, &accepted));
    assert(!sony_set_scalar(0, 42, SONY_DPC_FOCUS_MODE, 4, 1, NULL));
    assert(!sony_set_exposure_mode(0, 42, 1, NULL) && used == 0 && depth == 0);
    for (unsigned full = 0; full < 2; ++full) {
        for (unsigned pressed = 0; pressed < 2; ++pressed) {
            used = 0;
            assert(sony_shutter_button(0, 42, full, pressed, &accepted) && accepted);
            assert(used == 68 && get16(sent + 12) == SONY_OC_SET_CONTROL_DEVICE_B);
            assert(get32(sent + 18) == (full ? SONY_DPC_SHUTTER_RELEASE : SONY_DPC_SHUTTER_HALF_RELEASE));
            assert(get32(sent + 34) == 2 && get16(sent + 54) == (pressed ? 2 : 1));
        }
    }
    for (unsigned recording = 0; recording < 2; ++recording) {
        used = 0;
        assert(sony_movie_record(0, 42, recording, &accepted) && accepted);
        assert(used == 68 && get32(sent + 18) == SONY_DPC_MOVIE_RECORD);
        assert(get16(sent + 54) == (recording ? 2 : 1));
    }
    for (int direction = -1; direction <= 1; ++direction) {
        used = 0;
        assert(sony_zoom(0, 42, direction, &accepted) && accepted);
        assert(used == 67 && get32(sent + 18) == SONY_DPC_ZOOM_OPERATION);
        assert(get32(sent + 34) == 1 && sent[54] == (uint8_t)(int8_t)direction);
    }
    used = 0;
    assert(!sony_zoom(0, 42, 2, &accepted) && !accepted && !used);
    assert(!sony_shutter_button(0, 42, true, true, NULL));
    for (unsigned property = 0; property < 2; ++property) {
        for (int direction = -1; direction <= 1; direction += 2) {
            used = 0;
            uint16_t code = property ? SONY_DPC_F_NUMBER : SONY_DPC_SHUTTER_SPEED;
            assert(sony_setting_step(0, 42, code, direction, &accepted) && accepted);
            assert(used == 67 && get16(sent + 12) == SONY_OC_SET_CONTROL_DEVICE_B);
            assert(get32(sent + 18) == code && get32(sent + 34) == 1);
            assert(sent[54] == (uint8_t)(int8_t)direction);
        }
    }
    used = 0;
    assert(!sony_setting_step(0, 42, SONY_DPC_ISO, 1, &accepted) && !accepted && !used);
    assert(!sony_setting_step(0, 42, SONY_DPC_F_NUMBER, 0, &accepted) && !used);
    puts("Sony write wire tests passed");
    return 0;
}
