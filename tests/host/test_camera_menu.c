#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camera_menu.h"
#include "camera_menu_sony_legacy.h"
#include "sony_props.h" /* Original dataset fixture aliases generic choice state. */
#include "ptpip_packet.h"
#include "sony_codes.h"

static uint8_t data[512];
static size_t used;
static void begin(void) { memset(data, 0, sizeof(data)); used = 8; }
static void property(unsigned index, uint8_t enabled, uint32_t actual, const uint32_t *choices, unsigned count)
{
    static const uint16_t types[] = {6, 4, 6, 3, 4, 4, 4, 2, 6, 4, 4, 4, 2, 4, 4, 4};
    uint16_t type = types[index]; unsigned width = 1u << ((type - 1) / 2);
    put32(data, get32(data) + 1);
    data[used++] = camera_menu_codes[index]; data[used++] = camera_menu_codes[index] >> 8;
    data[used++] = type; data[used++] = type >> 8; data[used++] = 1; data[used++] = enabled;
    memset(data + used, 0, width); used += width;
    put32(data + used, actual); used += width; data[used++] = 2;
    data[used++] = count; data[used++] = count >> 8;
    for (unsigned i = 0; i < count; ++i) { put32(data + used, choices[i]); used += width; }
}
static void one(unsigned index, uint8_t enabled, uint32_t actual, const uint32_t *choices, unsigned count)
{ begin(); property(index, enabled, actual, choices, count); }
static void test_ev_direction(void)
{
    const uint32_t orders[][3] = {{0xfc18, 0, 1000}, {1000, 0, 0xfc18}, {0, 1000, 0xfc18}};
    const struct { uint32_t current; int direction; uint32_t target; } cases[] = {
        {0xfc18, 1, 0}, {0, 1, 1000}, {1000, -1, 0}, {0, -1, 0xfc18},
        {1000, 1, 0xfc18}, {0xfc18, -1, 1000},
    };
    for (unsigned order = 0; order < 3; ++order) {
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            camera_menu_t menu = {0}; camera_menu_write_t write;
            one(MENU_EV, 1, cases[i].current, orders[order], 3);
            assert(camera_menu_snapshot(&menu, data, used, 0));
            assert(camera_menu_step(&menu, MENU_EV, cases[i].direction, true));
            assert(camera_menu_next(&menu, MENU_EV, 1, &write));
            assert(write.type == 3 && write.value == cases[i].target);
        }
    }
    /* Merge around the desired EV while awaiting readback, even if the
     * next snapshot reports the same choices in a different order. */
    camera_menu_t menu = {0}; camera_menu_write_t write;
    one(MENU_EV, 1, 0, orders[1], 3);
    assert(camera_menu_snapshot(&menu, data, used, 0));
    assert(camera_menu_step(&menu, MENU_EV, 1, true));
    assert(camera_menu_next(&menu, MENU_EV, 1, &write) && write.value == 1000);
    camera_menu_response(&menu, MENU_EV, true);
    assert(camera_menu_step(&menu, MENU_EV, 1, true));
    one(MENU_EV, 1, 0, orders[2], 3);
    assert(camera_menu_snapshot(&menu, data, used, 2));
    assert(!camera_menu_next(&menu, MENU_EV, 2, &write));
    one(MENU_EV, 1, 1000, orders[0], 3);
    assert(camera_menu_snapshot(&menu, data, used, 3));
    assert(camera_menu_next(&menu, MENU_EV, 3, &write) && write.value == 0xfc18);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    test_ev_direction();
    camera_menu_t menu = {0}; camera_menu_write_t write; uint32_t target;
    uint32_t choices[] = {100, 200, 400, 800};
    one(MENU_ISO, 1, 200, choices, 4);
    assert(camera_menu_snapshot(&menu, data, used, 0));
    assert(camera_menu_step(&menu, MENU_ISO, 100, false));
    assert(camera_menu_target(&menu, MENU_ISO, &target) && target == 800);
    assert(camera_menu_next(&menu, MENU_ISO, 0, &write));
    assert(write.code == SONY_DPC_ISO && write.type == 6 && write.value == 800 && !write.relative);
    camera_menu_response(&menu, MENU_ISO, true);
    assert(camera_menu_snapshot(&menu, data, used, 5000));
    assert(camera_menu_status(&menu, MENU_ISO) == SETTING_PENDING);
    assert(camera_menu_step(&menu, MENU_ISO, -1, false));
    assert(!camera_menu_next(&menu, MENU_ISO, 5001, &write));
    one(MENU_ISO, 1, 800, choices, 4); assert(camera_menu_snapshot(&menu, data, used, 6000));
    assert(camera_menu_next(&menu, MENU_ISO, 6000, &write) && write.value == 400);
    camera_menu_response(&menu, MENU_ISO, false);
    assert(camera_menu_status(&menu, MENU_ISO) == SETTING_REJECTED && !camera_menu_pending(&menu));
    assert(camera_menu_step(&menu, MENU_ISO, 1, true));
    assert(camera_menu_target(&menu, MENU_ISO, &target) && target == 100);
    one(MENU_ISO, 2, 800, choices, 4); camera_menu_snapshot(&menu, data, used, 6001);
    assert(!camera_menu_pending(&menu) && !camera_menu_step(&menu, MENU_ISO, 1, false));
    /* Signed EV bit patterns are preserved. */
    uint32_t ev[] = {0xfc18, 0, 1000};
    one(MENU_EV, 1, 0, ev, 3); camera_menu_snapshot(&menu, data, used, 7000);
    assert(camera_menu_step(&menu, MENU_EV, -1, false));
    assert(camera_menu_next(&menu, MENU_EV, 7000, &write) && write.type == 3 && write.value == 0xfc18);
    camera_menu_cancel(&menu); assert(!camera_menu_pending(&menu));
    one(MENU_APERTURE, 1, 560, NULL, 0); camera_menu_snapshot(&menu, data, used, 10018);
    camera_menu_step(&menu, MENU_APERTURE, 2, false); camera_menu_next(&menu, MENU_APERTURE, 10018, &write);
    one(MENU_APERTURE, 1, 500, NULL, 0); camera_menu_snapshot(&menu, data, used, 10019);
    assert(camera_menu_status(&menu, MENU_APERTURE) == SETTING_REJECTED && !camera_menu_pending(&menu));
    /* Relative readback gates each step and merges opposite requests. */
    one(MENU_APERTURE, 1, 560, NULL, 0); camera_menu_snapshot(&menu, data, used, UINT32_MAX - 5);
    assert(camera_menu_step(&menu, MENU_APERTURE, 3, false));
    assert(!camera_menu_target(&menu, MENU_APERTURE, &target));
    assert(camera_menu_next(&menu, MENU_APERTURE, UINT32_MAX - 5, &write) && write.relative && write.value == 1);
    camera_menu_response(&menu, MENU_APERTURE, true);
    camera_menu_step(&menu, MENU_APERTURE, -2, false);
    camera_menu_snapshot(&menu, data, used, 1);
    assert(!camera_menu_next(&menu, MENU_APERTURE, 1, &write));
    one(MENU_APERTURE, 1, 630, NULL, 0); camera_menu_snapshot(&menu, data, used, 2);
    assert(camera_menu_status(&menu, MENU_APERTURE) == SETTING_APPLIED && !camera_menu_pending(&menu));
    camera_menu_step(&menu, MENU_APERTURE, -1, false);
    assert(camera_menu_next(&menu, MENU_APERTURE, 10, &write) && write.value == 255);
    camera_menu_response(&menu, MENU_APERTURE, true);
    camera_menu_snapshot(&menu, data, used, 10010);
    assert(camera_menu_status(&menu, MENU_APERTURE) == SETTING_TIMEOUT && !camera_menu_pending(&menu));
    camera_menu_step(&menu, MENU_APERTURE, 1, false);
    camera_menu_next(&menu, MENU_APERTURE, 10011, &write);
    camera_menu_response(&menu, MENU_APERTURE, false);
    assert(camera_menu_status(&menu, MENU_APERTURE) == SETTING_REJECTED);
    /* Missing, sentinel and duplicate descriptors never authorize a write. */
    one(MENU_SHUTTER, 1, UINT32_MAX, NULL, 0); camera_menu_snapshot(&menu, data, used, 10012);
    assert(!camera_menu_step(&menu, MENU_SHUTTER, 1, false));
    one(MENU_APERTURE, 1, 0xfffe, NULL, 0); camera_menu_snapshot(&menu, data, used, 10013);
    assert(!camera_menu_step(&menu, MENU_APERTURE, 1, false));
    one(MENU_WB, 1, 100, choices, 4); property(MENU_WB, 1, 100, choices, 4);
    assert(!camera_menu_snapshot(&menu, data, used, 10014));
    assert(!camera_menu_step(&menu, MENU_WB, 1, false));
    assert(!camera_menu_snapshot(&menu, data, used - 1, 10015));
    uint32_t oversized[65];
    for (unsigned i = 0; i < 65; ++i) oversized[i] = 100 + i;
    one(MENU_APERTURE, 1, 100, oversized, 65);
    assert(camera_menu_snapshot(&menu, data, used, 10016));
    assert(!menu.items[MENU_APERTURE].writable);
    assert(!camera_menu_step(&menu, MENU_APERTURE, 1, false));
    menu = (camera_menu_t){0};
    one(MENU_SHUTTER, 1, 0x00010064, NULL, 0);
    assert(camera_menu_snapshot(&menu, data, used, 10017));
    assert(camera_menu_status(&menu, MENU_SHUTTER) == SETTING_IDLE);
    assert(camera_menu_step(&menu, MENU_SHUTTER, 1, false));
    assert(camera_menu_next(&menu, MENU_SHUTTER, 10017, &write) && write.relative);
    camera_menu_cancel(&menu); assert(!camera_menu_pending(&menu));
    /* Every enum-backed parameter wraps both boundaries and retains readback gating. */
    uint32_t boundary_choices[] = {1, 2, 3, 4};
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        menu = (camera_menu_t){0};
        one(i, 1, boundary_choices[0], boundary_choices, 4);
        assert(camera_menu_snapshot(&menu, data, used, 11000));
        assert(camera_menu_step(&menu, i, -1, true));
        assert(camera_menu_target(&menu, i, &target) && target == boundary_choices[3]);
        assert(camera_menu_next(&menu, i, 11000, &write) && write.value == boundary_choices[3] && !write.relative);
        camera_menu_response(&menu, i, true);
        assert(camera_menu_snapshot(&menu, data, used, 11001));
        assert(camera_menu_status(&menu, i) == SETTING_PENDING);
        assert(camera_menu_step(&menu, i, 1, true));
        assert(!camera_menu_next(&menu, i, 11002, &write));
        one(i, 1, boundary_choices[3], boundary_choices, 4);
        assert(camera_menu_snapshot(&menu, data, used, 11003));
        assert(camera_menu_next(&menu, i, 11003, &write) && write.value == boundary_choices[0]);
        camera_menu_response(&menu, i, true);
        one(i, 1, boundary_choices[0], boundary_choices, 4);
        assert(camera_menu_snapshot(&menu, data, used, 11004));
        assert(camera_menu_status(&menu, i) == SETTING_APPLIED && !camera_menu_pending(&menu));
    }
    FILE *file = fopen(argv[1], "rb"); assert(file);
    uint8_t fixture[16384]; size_t size = fread(fixture, 1, sizeof(fixture), file); assert(feof(file)); fclose(file);
    assert(camera_menu_snapshot(&menu, fixture, size, 20000));
    for (unsigned i = MENU_ISO; i < MENU_ASPECT; ++i) {
        assert(menu.items[i].writable);
        sony_mode_state_t *state = &menu.items[i].control.snapshot;
        int direction = state->current == state->values[state->count - 1] ? -1 : 1;
        assert(camera_menu_step(&menu, i, direction, false));
        assert(camera_menu_next(&menu, i, 20000, &write) && !write.relative);
    }
    assert(!menu.items[MENU_SHUTTER].writable && !menu.items[MENU_APERTURE].writable);
    camera_menu_cancel(&menu); assert(!camera_menu_pending(&menu));
    for (unsigned i = MENU_ASPECT; i < CAMERA_MENU_COUNT; ++i) {
        menu = (camera_menu_t){0};
        one(i, 2, 1, boundary_choices, 4);
        assert(camera_menu_snapshot(&menu, data, used, 21000));
        assert(!menu.items[i].writable && !camera_menu_step(&menu, i, 1, true));
        one(i, 1, 1, NULL, 0);
        assert(camera_menu_snapshot(&menu, data, used, 21001));
        assert(!menu.items[i].writable && !menu.items[i].relative);
        assert(!camera_menu_step(&menu, i, 1, true));
        one(i, 1, 99, boundary_choices, 4);
        assert(camera_menu_snapshot(&menu, data, used, 21002));
        assert(!camera_menu_step(&menu, i, 1, true));
    }
    puts("camera menu capability, target merge, readback and safety tests passed");
    return 0;
}
