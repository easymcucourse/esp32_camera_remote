#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "sony_props.h"
#include "sony_codes.h"
#include "ptpip_packet.h"

int main(int argc, char **argv)
{
    uint8_t data[64] = {0};
    put32(data, 2);
    /* Two scalar records without forms. */
    data[8] = 0x0a; data[9] = 0x50; data[10] = 4;
    data[14] = 1; data[16] = 1;
    data[19] = 0x5b; data[20] = 0xd2; data[21] = 2;
    sony_focus_caps_t caps;
    assert(sony_parse_focus_caps(data, 28, &caps));
    assert(caps.focus_known && caps.focus_mode == SONY_FOCUS_MODE_MANUAL);
    assert(caps.zoom_known && caps.zoom_enabled == 0);
    data[26] = 1;
    assert(sony_parse_focus_caps(data, 28, &caps) && caps.zoom_enabled == 1);
    data[26] = 7;
    assert(sony_parse_focus_caps(data, 28, &caps) && !caps.zoom_known);
    for (size_t n = 0; n < 28; ++n) {
        assert(!sony_parse_focus_caps(data, n, &caps));
        assert(!caps.focus_known && !caps.zoom_known);
    }
    assert(!sony_parse_focus_caps(data, 29, &caps)); // trailing bytes
    put32(data, 0xffffffffu);
    assert(!sony_parse_focus_caps(data, 28, &caps));
    // False header in another property's scalar payload must not be scanned.
    put32(data, 1); data[8] = 0x44; data[9] = 0xd2; data[10] = 10;
    for (size_t i = 14; i < 46; ++i) data[i] = 0;
    data[16] = 0x0a; data[17] = 0x50; data[18] = 4;
    assert(sony_parse_focus_caps(data, 47, &caps));
    assert(!caps.focus_known && !caps.zoom_known);
    if (argc == 2) {
        FILE *f = fopen(argv[1], "rb"); assert(f);
        uint8_t sample[16384]; size_t n = fread(sample, 1, sizeof(sample), f);
        assert(feof(f)); fclose(f);
        assert(sony_parse_focus_caps(sample, n, &caps));
        printf("capture dataset=%zu focus_known=%d focus=%u zoom_known=%d zoom=%u\n",
               n, caps.focus_known, caps.focus_mode, caps.zoom_known, caps.zoom_enabled);
        for (size_t truncated = 0; truncated < n; ++truncated)
            assert(!sony_parse_focus_caps(sample, truncated, &caps));
    }
    puts("focus capability tests passed");
    return 0;
}
