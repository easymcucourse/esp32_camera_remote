#pragma once
#include "esp_err.h"
#include "matrix_model.h"
esp_err_t matrix_status_init(void);
void matrix_status_boot_stage(matrix_boot_t stage);
void matrix_status_hid_result(bool success);
void matrix_status_host_task_started(void);
void matrix_status_note_lcd_command(bool valid);
void matrix_status_set_ble_gamepad(matrix_link_t link);
void matrix_status_set_ble_gimbal(matrix_link_t link);
void matrix_status_set_gimbal_fault(bool active);
/* 0..100 percent, >100 unknown. Link disconnect clears cached capacity. */
void matrix_status_set_ble_gamepad_battery(uint8_t percent);
void matrix_status_set_ble_gimbal_battery(uint8_t percent);
uint8_t matrix_status_faults(void);
void matrix_status_get_state(matrix_model_t *out);
bool matrix_status_debug_command(int argc, char **argv);
void matrix_status_debug_get(bool *calibration, uint8_t *forced);
