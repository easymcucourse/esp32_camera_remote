#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "sony_props.h"
#include "sony_codes.h"
#include "ptpip_packet.h"

static unsigned seen;
static void observe(void *ctx, const sony_property_desc_t *d)
{
    (void)ctx;
    ++seen;
    if (d->code == SONY_DPC_EXPOSURE_PROGRAM) {
        uint32_t value;
        assert(d->type == 6 && d->scalar && d->value == 2 && d->writable);
        assert(d->choice_count == 3 && d->second_choice_count == 2);
        assert(sony_descriptor_choice(d, 2, &value) && value == 3);
        assert(!sony_descriptor_choice(d, 3, &value));
        assert(get32(d->second_choices) == 0x10020 && get32(d->second_choices + 4) == 3);
    } else {
        assert(d->code == 0xee01 && !d->scalar && !d->writable);
        assert(d->current_size == 8 && get32(d->current) == 2);
    }
}
int main(void)
{
    uint8_t data[96] = {0};
    put32(data, 2);
    /* UINT32 exposure enum with both observed Sony lists. */
    uint8_t *p = data + 8;
    p[0] = 0x0e; p[1] = 0x50; p[2] = 6; p[4] = 1; p[5] = 1;
    put32(p + 6, 1); put32(p + 10, 2); p[14] = 2; p[15] = 3;
    put32(p + 17, 1); put32(p + 21, 2); put32(p + 25, 3);
    p[29] = 2; put32(p + 31, 0x10020); put32(p + 35, 3);
    /* Unknown property with known array type; embedded false Mode header. */
    p = data + 47;
    p[0] = 1; p[1] = 0xee; p[2] = 4; p[3] = 0x40;
    put32(p + 6, 2); p[10] = 0x0e; p[11] = 0x50; p[12] = 6;
    put32(p + 14, 2); p[18] = 3; p[20] = 4;
    size_t size = 70;
    assert(sony_parse_descriptors(data, size, observe, NULL) && seen == 2);
    for (size_t n = 0; n < size; ++n) {
        seen = 0;
        assert(!sony_parse_descriptors(data, n, observe, NULL) && seen == 0);
    }
    seen = 0;
    assert(!sony_parse_descriptors(data, size + 1, observe, NULL) && seen == 0);
    sony_mode_state_t mode;
    sony_parse_properties(data, size, &mode, NULL, NULL);
    assert(mode.count == 3 && mode.current == 2 && mode.writable);
    /* Oversized array, unsupported type, unknown form never publish. */
    put32(p + 14, UINT32_MAX);
    assert(!sony_parse_descriptors(data, size, observe, NULL));
    put32(p + 14, 2); p[2] = 0x20;
    assert(!sony_parse_descriptors(data, size, observe, NULL));
    p[2] = 4; p[22] = 3;
    assert(!sony_parse_descriptors(data, size, observe, NULL));
    p[22] = 0;
    /* High get/set bit is not treated as write authorization. */
    data[12] = 0x81;
    sony_parse_properties(data, size, &mode, NULL, NULL);
    assert(mode.count == 3 && !mode.writable);
    /* Duplicate control descriptor invalidates the entire snapshot. */
    p[0] = 0x0e; p[1] = 0x50;
    assert(!sony_parse_descriptors(data, size, NULL, NULL));
    assert(seen == 0);
    puts("Sony descriptor boundary tests passed");
    return 0;
}
