#include "ds4_report.h"

/* DS4 HID reports include their report ID, but no HID transaction byte.
 * Layout references: Linux hid-playstation and Bluepad32 DS4 parser. */
bool ds4_parse_report(const uint8_t *data, size_t length, ds4_state_t *out)
{
    if (!data || !out || length == 0) return false;
    size_t offset;
    uint8_t battery = 255;
    if (data[0] == 0x11 && length == 78) {
        offset = 3;
        battery = data[32] & 0x0f;
    } else if (data[0] == 0x01 && (length == 10 || length == 64)) {
        offset = 1;
        if (length == 64) battery = data[30] & 0x0f;
    } else return false;
    const uint8_t *p = data + offset;
    uint32_t buttons = 0;
    const uint8_t hat = p[4] & 0x0f;
    if (hat == 0 || hat == 1 || hat == 7) buttons |= 1u << 4;
    if (hat == 1 || hat == 2 || hat == 3) buttons |= 1u << 5;
    if (hat == 3 || hat == 4 || hat == 5) buttons |= 1u << 6;
    if (hat == 5 || hat == 6 || hat == 7) buttons |= 1u << 7;
    if (p[4] & 0x10) buttons |= 1u << 15;
    if (p[4] & 0x20) buttons |= 1u << 14;
    if (p[4] & 0x40) buttons |= 1u << 13;
    if (p[4] & 0x80) buttons |= 1u << 12;
    if (p[5] & 0x01) buttons |= 1u << 10;
    if (p[5] & 0x02) buttons |= 1u << 11;
    if (p[5] & 0x04) buttons |= 1u << 8;
    if (p[5] & 0x08) buttons |= 1u << 9;
    if (p[5] & 0x10) buttons |= 1u << 0;
    if (p[5] & 0x20) buttons |= 1u << 3;
    if (p[5] & 0x40) buttons |= 1u << 1;
    if (p[5] & 0x80) buttons |= 1u << 2;
    if (p[6] & 0x01) buttons |= 1u << 16;
    if (p[6] & 0x02) buttons |= 1u << 17;
    *out = (ds4_state_t){.connected = true, .buttons = buttons,
        .lx = (int)p[0] - 128, .ly = (int)p[1] - 128,
        .rx = (int)p[2] - 128, .ry = (int)p[3] - 128,
        .l2 = p[7], .r2 = p[8], .battery = battery};
    return true;
}
