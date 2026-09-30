#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ds4_report.h"

int main(void)
{
    ds4_state_t s = {0};
    uint8_t short_report[10] = {1, 128, 128, 128, 128, 8, 0, 0, 0, 0};
    assert(ds4_parse_report(short_report, sizeof(short_report), &s));
    assert(s.connected && !s.buttons && !s.lx && !s.ly && !s.rx && !s.ry && s.battery == 255);
    const uint32_t hats[] = {1u<<4, (1u<<4)|(1u<<5), 1u<<5, (1u<<5)|(1u<<6),
        1u<<6, (1u<<6)|(1u<<7), 1u<<7, (1u<<7)|(1u<<4), 0, 0, 0, 0, 0, 0, 0, 0};
    for (int hat = 0; hat < 16; ++hat) {
        short_report[5] = hat;
        assert(ds4_parse_report(short_report, 10, &s) && s.buttons == hats[hat]);
    }
    short_report[5] = 0xf8;
    short_report[6] = 0xff;
    short_report[7] = 0xff; /* sequence counter bits must not become buttons */
    short_report[1] = 0; short_report[2] = 255;
    short_report[8] = 17; short_report[9] = 255;
    assert(ds4_parse_report(short_report, 10, &s));
    assert(s.buttons == (0x3ffffu & ~0xf0u));
    assert(s.lx == -128 && s.ly == 127 && s.l2 == 17 && s.r2 == 255);
    uint8_t full[78] = {0x11, 0xc0, 0x00};
    memcpy(full + 3, short_report + 1, 9);
    full[32] = 0x17;
    assert(ds4_parse_report(full, 78, &s) && s.battery == 7 && s.l2 == 17 && s.lx == -128);
    uint8_t usb[64] = {1};
    memcpy(usb + 1, short_report + 1, 9);
    usb[30] = 0x15;
    assert(ds4_parse_report(usb, 64, &s) && s.battery == 5);
    ds4_state_t before = s;
    assert(!ds4_parse_report(full, 77, &s));
    assert(!ds4_parse_report(short_report, 9, &s));
    full[0] = 0x31; /* DualSense */
    assert(!ds4_parse_report(full, 78, &s));
    assert(!ds4_parse_report(NULL, 10, &s));
    assert(!ds4_parse_report(full, 0, &s));
    assert(!ds4_parse_report(short_report, 10, NULL));
    assert(memcmp(&before, &s, sizeof(s)) == 0);
    puts("DS4 report tests passed");
    return 0;
}
