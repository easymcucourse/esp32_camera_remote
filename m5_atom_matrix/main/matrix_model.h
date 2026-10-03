#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { MATRIX_DISCONNECTED, MATRIX_SEARCHING, MATRIX_CONNECTING, MATRIX_CONNECTED } matrix_link_t;
typedef enum { MATRIX_BOOT_LED, MATRIX_BOOT_PERIPHERALS, MATRIX_BOOT_STORAGE,
    MATRIX_BOOT_BLUETOOTH, MATRIX_BOOT_HID_HOST, MATRIX_BOOT_DONE } matrix_boot_t;
typedef enum { MATRIX_OFF, MATRIX_WHITE, MATRIX_GREEN, MATRIX_YELLOW, MATRIX_ORANGE,
    MATRIX_RED, MATRIX_BLUE, MATRIX_CYAN, MATRIX_MAGENTA } matrix_color_t;
enum { MATRIX_OVERFLOW = 1, MATRIX_PROTOCOL = 2, MATRIX_BLUETOOTH = 4 };
typedef struct {
    matrix_boot_t boot;
    uint32_t started_ms, done_ms, lcd_ms, overflow_ms, dropped;
    uint32_t bad_times[3], hid_wait_ms;
    uint8_t bad_count, good_count, faults;
    bool lcd_seen, overflow_active, hid_waiting, hid_result, hid_ok, host_task;
    matrix_link_t classic, ble_pad, gimbal;
    uint8_t classic_battery, ble_battery, gimbal_battery; /* Percent; 255 unknown. */
} matrix_model_t;
typedef struct { bool sent; uint8_t last[25]; uint32_t last_ms; } matrix_frame_cache_t;
void matrix_model_init(matrix_model_t *model, uint32_t now);
void matrix_model_stage(matrix_model_t *model, matrix_boot_t stage, uint32_t now);
void matrix_model_hid_result(matrix_model_t *model, bool success, uint32_t now);
void matrix_model_host_task(matrix_model_t *model, uint32_t now);
void matrix_model_i2c(matrix_model_t *model, bool valid, uint32_t now);
void matrix_model_dropped(matrix_model_t *model, uint32_t dropped, uint32_t now);
void matrix_model_tick(matrix_model_t *model, uint32_t now);
void matrix_model_frame(const matrix_model_t *model, uint32_t now, uint8_t colors[25]);
bool matrix_frame_due(const matrix_frame_cache_t *cache, const uint8_t colors[25], uint32_t now);
void matrix_frame_sent(matrix_frame_cache_t *cache, const uint8_t colors[25], uint32_t now);
void matrix_frame_grb(const uint8_t colors[25], uint8_t grb[75]);
