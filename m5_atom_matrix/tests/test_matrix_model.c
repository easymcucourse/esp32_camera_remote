#include "matrix_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t frame[25];
static void expect(matrix_model_t *m, uint32_t now, const char *picture)
{
    static const char palette[] = ".wgyorbcm";
    assert(strlen(picture) == 25);
    matrix_model_frame(m, now, frame);
    for (unsigned i = 0; i < 25; ++i) assert(palette[frame[i]] == picture[i]);
}
static matrix_model_t run(uint32_t start)
{
    matrix_model_t m; matrix_model_init(&m, start);
    matrix_model_stage(&m, MATRIX_BOOT_DONE, start);
    return m;
}
int main(void)
{
    matrix_model_t m; matrix_model_init(&m, 0);
    expect(&m, 0,   "ww...ww...ww...ww...ww...");
    expect(&m, 250, "w....w....w....w....w....");
    matrix_model_stage(&m, MATRIX_BOOT_STORAGE, 250);
    expect(&m, 250, "ww...ww...ww...ww...ww...");
    expect(&m, 500, "www..www..www..www..www..");
    matrix_model_stage(&m, MATRIX_BOOT_BLUETOOTH, 500);
    expect(&m, 500, "wwww.wwww.wwww.wwww.wwww.");
    matrix_model_stage(&m, MATRIX_BOOT_HID_HOST, 500);
    expect(&m, 750, "wwww.wwww.wwww.wwww.wwww.");
    matrix_model_stage(&m, MATRIX_BOOT_STORAGE, 750); assert(m.boot == MATRIX_BOOT_HID_HOST);
    m.faults = MATRIX_BLUETOOTH | MATRIX_PROTOCOL | MATRIX_OVERFLOW;
    matrix_model_stage(&m, MATRIX_BOOT_DONE, 1000);
    expect(&m, 1299, "wwwwwwwwwwwwwwwwwwwwwwwww");
    expect(&m, 1300, "mmmm.m...mmmmm.m...mmmmm.");
    expect(&m, 1800, "mmmm.m...mmmmm.m...mmmmm.");
    m.faults &= ~MATRIX_OVERFLOW;
    expect(&m, 1800, "mmmm.m...mmmmm.m...mmmmm.");
    m.faults &= ~MATRIX_BLUETOOTH;
    expect(&m, 1800, "..o....o....o.........o..");
    m.faults = MATRIX_OVERFLOW;
    expect(&m, 1800, "yyyyy.....yyyyy.....yyyyy");

    m = run(0); m.classic = m.ble_pad = m.gimbal = MATRIX_CONNECTED;
    matrix_model_i2c(&m, true, 1000);
    expect(&m, 1000, "....................g.bcm");
    expect(&m, 2500, "....................g.bcm");
    expect(&m, 2501, "....................r.bcm");
    matrix_model_i2c(&m, true, 3000); m.classic = MATRIX_SEARCHING;
    m.ble_pad = MATRIX_CONNECTING; m.gimbal = MATRIX_DISCONNECTED;
    expect(&m, 4000, "....................g.bc.");
    expect(&m, 4124, "....................g.bc.");
    expect(&m, 4125, "....................g..c.");
    expect(&m, 4250, "....................g....");
    expect(&m, 4500, "....................g..c.");

    m = run(0);
    for (unsigned i = 3; i < 17; ++i) {
        matrix_model_frame(&m, i * 125, frame);
        for (unsigned p = 0; p < 20; ++p) assert(frame[p] == (p == 15 + i % 5 ? MATRIX_YELLOW : MATRIX_OFF));
        assert(frame[20] == (i % 4 < 2 ? MATRIX_YELLOW : MATRIX_OFF));
        for (unsigned p = 21; p < 25; ++p) assert(frame[p] == MATRIX_OFF);
    }
    matrix_model_frame(&m, 10000, frame); assert(frame[20] == MATRIX_YELLOW);
    expect(&m, 10001, "....................r....");

    /* Each device's battery occupies its own row; disconnected/unknown is dark. */
    m = run(0); matrix_model_i2c(&m, true, 1000);
    m.classic = m.ble_pad = m.gimbal = MATRIX_CONNECTED;
    m.classic_battery = 100; m.ble_battery = 60; m.gimbal_battery = 21;
    expect(&m, 1000, "bbbbbccc..mm........g.bcm");
    for (unsigned percent = 21; percent <= 100; ++percent) {
        m.classic_battery = (uint8_t)percent; matrix_model_frame(&m, 1000, frame);
        for (unsigned x = 0; x < 5; ++x) assert(frame[x] == (x < (percent + 19) / 20 ? MATRIX_BLUE : MATRIX_OFF));
    }
    m.classic_battery = 20; m.ble_battery = 0; m.gimbal_battery = 255;
    expect(&m, 1000, "r....r..............g.bcm");
    expect(&m, 1250, "....................g.bcm");
    m.classic = MATRIX_DISCONNECTED; m.ble_pad = MATRIX_SEARCHING;
    expect(&m, 1000, "....................g...m");
    m.faults = MATRIX_PROTOCOL;
    expect(&m, 1000, "..o....o....o.........o..");

    m = run(0);
    matrix_model_i2c(&m, false, 0); matrix_model_i2c(&m, false, 1000);
    assert(!(m.faults & MATRIX_PROTOCOL)); matrix_model_i2c(&m, false, 2000);
    assert(m.faults & MATRIX_PROTOCOL);
    matrix_model_i2c(&m, true, 2100); matrix_model_i2c(&m, true, 2200);
    assert(m.faults & MATRIX_PROTOCOL); matrix_model_i2c(&m, true, 2300);
    assert(!(m.faults & MATRIX_PROTOCOL)); matrix_model_i2c(&m, false, 2301);
    assert(!(m.faults & MATRIX_PROTOCOL));
    m = run(0);
    matrix_model_i2c(&m, false, 0); matrix_model_i2c(&m, false, 2001); matrix_model_i2c(&m, false, 3000);
    assert(!(m.faults & MATRIX_PROTOCOL));
    matrix_model_i2c(&m, true, 3500); matrix_model_i2c(&m, false, 4000);
    assert(m.faults & MATRIX_PROTOCOL); /* Three bad frames in a 2 s window, not necessarily consecutive. */
    m = run(0);
    matrix_model_i2c(&m, false, 0); matrix_model_i2c(&m, false, 1900);
    matrix_model_i2c(&m, false, 2100); assert(!(m.faults & MATRIX_PROTOCOL));
    matrix_model_i2c(&m, false, 2200); assert(m.faults & MATRIX_PROTOCOL); /* Sliding window, not fixed buckets. */

    m = run(UINT32_MAX - 100);
    matrix_model_i2c(&m, true, UINT32_MAX - 10);
    matrix_model_frame(&m, 1489, frame); assert(frame[20] == MATRIX_GREEN);
    matrix_model_frame(&m, 1490, frame); assert(frame[20] == MATRIX_RED);
    matrix_model_dropped(&m, 1, UINT32_MAX - 100);
    matrix_model_tick(&m, 4898); assert(m.faults & MATRIX_OVERFLOW);
    matrix_model_dropped(&m, 2, 4898); matrix_model_tick(&m, 9897); assert(m.faults & MATRIX_OVERFLOW);
    matrix_model_tick(&m, 9898); assert(!(m.faults & MATRIX_OVERFLOW));

    matrix_model_init(&m, 0); matrix_model_stage(&m, MATRIX_BOOT_HID_HOST, 10);
    matrix_model_hid_result(&m, true, 20); assert(m.boot != MATRIX_BOOT_DONE);
    matrix_model_host_task(&m, 30); assert(m.boot == MATRIX_BOOT_DONE && m.done_ms == 30);
    matrix_model_init(&m, 0); matrix_model_host_task(&m, 10);
    matrix_model_tick(&m, 3009); assert(m.hid_waiting && !(m.faults & MATRIX_BLUETOOTH));
    matrix_model_tick(&m, 3010); assert(m.boot == MATRIX_BOOT_DONE && (m.faults & MATRIX_BLUETOOTH));
    matrix_model_hid_result(&m, true, 3020); assert(m.faults & MATRIX_BLUETOOTH); /* Sticky until reboot. */
    matrix_model_init(&m, 0); matrix_model_hid_result(&m, false, 10); matrix_model_host_task(&m, 20);
    assert(m.boot == MATRIX_BOOT_DONE && (m.faults & MATRIX_BLUETOOTH));

    matrix_frame_cache_t cache = {0}; memset(frame, 0, sizeof(frame));
    assert(matrix_frame_due(&cache, frame, UINT32_MAX - 10));
    matrix_frame_sent(&cache, frame, UINT32_MAX - 10);
    assert(!matrix_frame_due(&cache, frame, 1988)); assert(matrix_frame_due(&cache, frame, 1989));
    frame[24] = MATRIX_CYAN; assert(matrix_frame_due(&cache, frame, 0));
    uint8_t grb[75]; matrix_frame_grb(frame, grb);
    assert(grb[72] == 10 && grb[73] == 0 && grb[74] == 10);
    for (unsigned i = 0; i < 72; ++i) assert(grb[i] == 0);
    for (unsigned color = 0; color <= MATRIX_MAGENTA; ++color) {
        memset(frame, color, sizeof(frame)); matrix_frame_grb(frame, grb);
        for (unsigned i = 0; i < 75; ++i) assert(grb[i] <= 20);
    }
    puts("Matrix reference patterns, priorities, timers and asynchronous startup passed");
}
