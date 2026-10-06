#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Call init once. Drawing/measurement must hold the board display mutex. */
esp_err_t ui_fonts_init(void);
int ui_fonts_measure(const char *text, int pixel_size, bool parameter_numbers);
int ui_fonts_line_height(int pixel_size);
void ui_fonts_draw(uint16_t *pixels, int width, int height, int left, int top,
                   const char *text, int pixel_size, uint16_t color,
                   bool parameter_numbers, int clip_right);
