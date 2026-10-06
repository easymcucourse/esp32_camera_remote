#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/* Selected hardware backend contract. Only display_surface may call it.
 * All operations are serialized by the surface. No application state here. */
typedef void (*display_backend_prepare_t)(uint16_t *pixels);
esp_err_t display_backend_init(display_backend_prepare_t prepare, esp_err_t (*prepare_resources)(void));
bool display_backend_ready(void);
void display_backend_dimensions(size_t *width, size_t *height, size_t *stride);
uint16_t *display_backend_back_buffer(void);
esp_err_t display_backend_publish(uint16_t *pixels);
esp_err_t display_backend_recover(display_backend_prepare_t prepare);
esp_err_t display_backend_test_fault(unsigned mode);
