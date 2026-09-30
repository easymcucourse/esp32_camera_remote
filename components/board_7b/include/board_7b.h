#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BOARD_LCD_WIDTH 1024
#define BOARD_LCD_HEIGHT 600

// One-time initialization. On failure, app_main aborts; no partial-init retry.
esp_err_t board_7b_init(const char *ssid, const char *password);
// Call only before starting the JPEG worker or after it has drained.
esp_err_t board_7b_show_connection(const char *status);
// Refresh connection-screen peripheral status; Atom loss also disconnects DS4.
void board_7b_set_atom_status(bool atom_online, bool controller_online);
// Live-view status shown in the upper-right corner.
void board_7b_set_wifi_rssi(int rssi);
void board_7b_set_camera_info(const char *model, const char *firmware);
void board_7b_set_exposure_mode(uint32_t mode);
void board_7b_set_camera_property(uint16_t code, uint32_t value);
bool board_7b_toggle_settings_mode(void);
// Single JPEG worker only; owns decoder state and framebuffer publication.
esp_err_t board_7b_show_jpeg(const uint8_t *jpeg, size_t length);
