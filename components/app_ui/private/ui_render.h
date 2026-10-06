#pragma once
#include "display_surface.h"
#include "ui_render_lifecycle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#define UI_CANVAS_WIDTH 1024
#define UI_CANVAS_HEIGHT 600
/* Component-private renderer collaboration; all drawing holds display_mutex. */
extern SemaphoreHandle_t display_mutex;
extern bool showing_connection;
bool ui_render_surface_ready(void);
esp_err_t ui_render_publish_frame(display_canvas_t *canvas);
void ui_render_draw_settings_panel(uint16_t *pixels);
void ui_render_draw_preview_status(uint16_t *pixels);
esp_err_t ui_jpeg_init(void);
void ui_jpeg_reset(void);
void ui_jpeg_reset_fps(void);
