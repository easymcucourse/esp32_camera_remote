#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "sony_props.h"
#include "sony_codes.h"

static unsigned calls;
static void observe(void *ctx, uint16_t code, uint32_t value)
{
    (void)ctx; (void)code; (void)value;
    ++calls;
}
int main(int argc, char **argv)
{
    assert(argc == 4);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    uint8_t data[16384]; size_t n = fread(data, 1, sizeof(data), f);
    assert(feof(f)); fclose(f);
    sony_focus_caps_t caps;
    assert(sony_parse_focus_caps(data, n, &caps));
    assert(caps.focus_known && caps.focus_mode == (uint16_t)strtoul(argv[2], NULL, 0));
    assert(caps.zoom_known && caps.zoom_enabled == (uint8_t)strtoul(argv[3], NULL, 0));
    sony_mode_state_t mode;
    sony_parse_properties(data, n, &mode, observe, NULL);
    assert(mode.count >= 2 && calls >= 9);
    unsigned index = 0;
    while (index < mode.count && mode.values[index] != mode.current) ++index;
    assert(index < mode.count);
    for (size_t truncated = 0; truncated < n; ++truncated) {
        calls = 0;
        assert(!sony_parse_scalar_properties(data, truncated, observe, NULL));
        assert(calls == 0);
        sony_parse_properties(data, truncated, &mode, observe, NULL);
        assert(!mode.count && !mode.writable && !calls);
    }
    printf("property fixture validated: %zu bytes, all truncations rejected\n", n);
    return 0;
}
