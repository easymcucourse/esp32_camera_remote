#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ptp_dataset.h"
#include "ptpip_packet.h"
#include "sony_props.h"
#include "sony_codes.h"

static size_t append_string(uint8_t *p, const char *text)
{
    size_t n = strlen(text) + 1;
    p[0] = (uint8_t)n;
    for (size_t i = 0; i < n; ++i) { p[1 + 2*i] = (uint8_t)text[i]; p[2 + 2*i] = 0; }
    return 1 + 2*n;
}

static void test_device_info(void)
{
    uint8_t data[256] = {0};
    size_t size = 8;
    size += append_string(data + size, "Sony");
    size += 2; // Functional mode.
    size += 5*4; // Five empty capability arrays.
    size += append_string(data + size, "Sony Corporation");
    size += append_string(data + size, "ZV-E10");
    size += append_string(data + size, "2.00");
    char model[24], firmware[24];
    assert(ptp_parse_device_info(data, size, model, firmware));
    assert(strcmp(model, "ZV-E10") == 0 && strcmp(firmware, "2.00") == 0);
    for (size_t n = 0; n < size; ++n)
        assert(!ptp_parse_device_info(data, n, model, firmware));
    // Existing ASCII fallback for non-ASCII UTF-16 code units.
    size_t model_offset = 8 + 11 + 2 + 5*4 + 35;
    data[model_offset + 2] = 1;
    assert(ptp_parse_device_info(data, size, model, firmware));
    assert(strcmp(model, "?V-E10") == 0);
    // Corrupt the first capability count; it must not overrun the dataset.
    put32(data + 8 + 11 + 2, 0xffffffffu);
    assert(!ptp_parse_device_info(data, size, model, firmware));
}

typedef struct { unsigned count; uint16_t code[9]; uint32_t value[9]; } observed_t;
static void observe(void *context, uint16_t code, uint32_t value)
{
    observed_t *o = context;
    assert(o->count < 9);
    o->code[o->count] = code;
    o->value[o->count++] = value;
}

static void test_properties(void)
{
    // Synthetic, identifier-free dataset using the existing Sony record layout.
    uint8_t data[64] = {0};
    put32(data, 2);
    uint8_t *p = data + 8;
    p[0] = 0x0e; p[1] = 0x50; p[2] = 6; p[4] = 1; p[5] = 1;
    put32(p + 6, 1); put32(p + 10, 2);
    p[14] = 2; p[15] = 3;
    put32(p + 17, 1); put32(p + 21, 2); put32(p + 25, 3);
    p = data + 37;
    p[0] = 7; p[1] = 0x50; p[2] = 4; p[4] = 1; p[5] = 1;
    p[6] = 0x18; p[7] = 1; p[8] = 0x90; p[9] = 1;
    size_t size = 48;
    sony_mode_state_t mode = {0};
    observed_t o = {0};
    sony_parse_properties(data, size, &mode, observe, &o);
    assert(mode.count == 3 && mode.writable && mode.current == 2);
    assert(mode.values[0] == 1 && mode.values[1] == 2 && mode.values[2] == 3);
    assert(o.count == 2 && o.code[0] == SONY_DPC_EXPOSURE_PROGRAM && o.value[0] == 2);
    assert(o.code[1] == SONY_DPC_F_NUMBER && o.value[1] == 400);
    // Preserve the old parser's partial-input behavior: current values may be
    // emitted even when the trailing enum choices are incomplete.
    o = (observed_t){0};
    sony_parse_properties(data, 25, &mode, observe, &o);
    assert(mode.count == 0 && !mode.writable && mode.current == 2);
    assert(o.count == 1 && o.value[0] == 2);
    put32(data + 4, 1);
    o = (observed_t){0};
    sony_parse_properties(data, size, &mode, observe, &o);
    assert(mode.count == 0 && !mode.writable && o.count == 0 && mode.current == 2);
    for (size_t n = 0; n <= size; ++n) {
        o = (observed_t){0};
        sony_parse_properties(data, n, &mode, observe, &o);
        assert(mode.count == 0 && o.count == 0);
    }
}

int main(void)
{
    test_device_info();
    test_properties();
    puts("camera parser regression tests passed");
    return 0;
}
