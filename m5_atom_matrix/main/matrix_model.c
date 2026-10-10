#include "matrix_model.h"
#include <string.h>

static void finish_boot(matrix_model_t *m, uint32_t now)
{
    if (m->boot != MATRIX_BOOT_DONE) { m->boot = MATRIX_BOOT_DONE; m->done_ms = now; }
}
void matrix_model_init(matrix_model_t *m, uint32_t now)
{ *m = (matrix_model_t){.boot = MATRIX_BOOT_PERIPHERALS, .started_ms = now,
    .classic_battery = 255, .ble_battery = 255, .gimbal_battery = 255}; }
void matrix_model_stage(matrix_model_t *m, matrix_boot_t stage, uint32_t now)
{
    if (stage < m->boot || stage > MATRIX_BOOT_DONE) return;
    if (stage == MATRIX_BOOT_DONE) finish_boot(m, now);
    else m->boot = stage;
}
void matrix_model_hid_result(matrix_model_t *m, bool success, uint32_t now)
{
    m->hid_result = true; m->hid_ok = success;
    if (!success) m->faults |= MATRIX_BLUETOOTH;
    if (m->host_task) { m->hid_waiting = false; finish_boot(m, now); }
}
void matrix_model_host_task(matrix_model_t *m, uint32_t now)
{
    m->host_task = true; m->hid_wait_ms = now;
    m->hid_waiting = !m->hid_result;
    if (m->hid_result) finish_boot(m, now);
}
void matrix_model_i2c(matrix_model_t *m, bool valid, uint32_t now)
{
    if (valid) {
        m->lcd_seen = true; m->lcd_ms = now;
        if (m->good_count < 3) ++m->good_count;
        if (m->good_count == 3) { m->faults &= ~MATRIX_PROTOCOL; m->bad_count = 0; }
    } else {
        m->good_count = 0;
        if (m->bad_count == 3) {
            m->bad_times[0] = m->bad_times[1]; m->bad_times[1] = m->bad_times[2];
        } else ++m->bad_count;
        m->bad_times[m->bad_count - 1] = now;
        if (m->bad_count == 3 && (uint32_t)(now - m->bad_times[0]) <= 2000)
            m->faults |= MATRIX_PROTOCOL;
    }
}
void matrix_model_dropped(matrix_model_t *m, uint32_t dropped, uint32_t now)
{
    if (m->dropped == dropped) return;
    m->dropped = dropped; m->overflow_active = true; m->overflow_ms = now;
    m->faults |= MATRIX_OVERFLOW;
}
void matrix_model_tick(matrix_model_t *m, uint32_t now)
{
    if (m->overflow_active && (uint32_t)(now - m->overflow_ms) >= 5000) {
        m->overflow_active = false; m->faults &= ~MATRIX_OVERFLOW;
    }
    if (m->hid_waiting && (uint32_t)(now - m->hid_wait_ms) >= 3000) {
        m->hid_waiting = false; m->faults |= MATRIX_BLUETOOTH; finish_boot(m, now);
    }
}
static bool link_on(matrix_link_t link, uint32_t elapsed)
{
    return link == MATRIX_CONNECTED || (link == MATRIX_CONNECTING && elapsed % 500 < 250) ||
        (link == MATRIX_SEARCHING && elapsed % 2000 < 125);
}
static void battery_row(uint8_t *row, matrix_link_t link, unsigned percent,
                        matrix_color_t color, bool blink)
{
    if (link != MATRIX_CONNECTED || percent > 100) return;
    unsigned count = (percent + 19) / 20; /* Five LEDs, from left to right. */
    if (percent <= 20) { color = MATRIX_RED; if (!blink) return; if (!count) count = 1; }
    for (unsigned x = 0; x < count; ++x) row[x] = color;
}
void matrix_model_frame(const matrix_model_t *m, uint32_t now, uint8_t out[25])
{
    memset(out, MATRIX_OFF, 25);
    uint32_t elapsed = now - m->started_ms;
    bool blink = elapsed % 500 < 250;
    if (m->boot != MATRIX_BOOT_DONE || (uint32_t)(now - m->done_ms) < 300) {
        for (unsigned y = 0; y < 5; ++y)
            for (unsigned x = 0; x < 5; ++x)
                if (x < (unsigned)m->boot || (x == (unsigned)m->boot && blink)) out[y * 5 + x] = MATRIX_WHITE;
        return;
    }
    if (m->faults & (MATRIX_BLUETOOTH | MATRIX_GIMBAL)) {
        const uint8_t rows[] = {0x0f, 0x11, 0x0f, 0x11, 0x0f};
        for (unsigned y = 0; y < 5; ++y) for (unsigned x = 0; x < 5; ++x)
            if (rows[y] & (1u << x)) out[y * 5 + x] = MATRIX_MAGENTA;
        return;
    }
    if (m->faults & MATRIX_PROTOCOL) {
        out[2] = out[7] = out[12] = out[22] = MATRIX_ORANGE; return;
    }
    if (m->faults & MATRIX_OVERFLOW) {
        for (unsigned y = 0; y < 5; y += 2) for (unsigned x = 0; x < 5; ++x) out[y * 5 + x] = MATRIX_YELLOW;
        return;
    }
    bool waiting = !m->lcd_seen && elapsed <= 10000;
    if (waiting) {
        out[15 + (elapsed / 125) % 5] = MATRIX_YELLOW;
        if (blink) out[20] = MATRIX_YELLOW;
    } else out[20] = m->lcd_seen && (uint32_t)(now - m->lcd_ms) <= 1500 ? MATRIX_GREEN : MATRIX_RED;
    if (link_on(m->classic, elapsed)) out[22] = MATRIX_BLUE;
    if (link_on(m->ble_pad, elapsed)) out[23] = MATRIX_CYAN;
    if (link_on(m->gimbal, elapsed)) out[24] = MATRIX_MAGENTA;
    battery_row(out, m->classic, m->classic_battery, MATRIX_BLUE, blink);
    battery_row(out + 5, m->ble_pad, m->ble_battery, MATRIX_CYAN, blink);
    battery_row(out + 10, m->gimbal, m->gimbal_battery, MATRIX_MAGENTA, blink);
}
bool matrix_frame_due(const matrix_frame_cache_t *c, const uint8_t colors[25], uint32_t now)
{ return !c->sent || memcmp(c->last, colors, 25) || (uint32_t)(now - c->last_ms) >= 2000; }
void matrix_frame_sent(matrix_frame_cache_t *c, const uint8_t colors[25], uint32_t now)
{ memcpy(c->last, colors, 25); c->last_ms = now; c->sent = true; }
void matrix_frame_grb(const uint8_t colors[25], uint8_t grb[75])
{
    static const uint8_t rgb[][3] = {{0,0,0},{6,6,6},{0,10,0},{10,8,0},{12,4,0},
        {12,0,0},{0,0,12},{0,10,10},{10,0,10}};
    static const uint8_t logical_to_physical[25] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24};
    for (unsigned i = 0; i < 25; ++i) {
        unsigned p = logical_to_physical[i]; unsigned c = colors[i] <= MATRIX_MAGENTA ? colors[i] : MATRIX_OFF;
        grb[p * 3] = rgb[c][1]; grb[p * 3 + 1] = rgb[c][0]; grb[p * 3 + 2] = rgb[c][2];
    }
}
