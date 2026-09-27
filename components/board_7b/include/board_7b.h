#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

#define BOARD_LCD_WIDTH 1024
#define BOARD_LCD_HEIGHT 600

// One-time initialization. On failure, app_main aborts; no partial-init retry.
esp_err_t board_7b_init(void);
// Single JPEG worker only; owns decoder state and framebuffer publication.
esp_err_t board_7b_show_jpeg(const uint8_t *jpeg, size_t length);
