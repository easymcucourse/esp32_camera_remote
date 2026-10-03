#pragma once
#include <stdbool.h>
#include "esp_lcd_panel_ops.h"
#define LCD_CLK_SRC_DEFAULT 0
typedef struct { int unused; } esp_lcd_rgb_panel_event_data_t;
typedef struct {
    bool (*on_frame_buf_complete)(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t *, void *);
} esp_lcd_rgb_panel_event_callbacks_t;
typedef struct {
    int clk_src;
    struct {
        int pclk_hz, h_res, v_res, hsync_pulse_width, hsync_back_porch, hsync_front_porch;
        int vsync_pulse_width, vsync_back_porch, vsync_front_porch;
        struct { bool pclk_active_neg; } flags;
    } timings;
    int data_width, bits_per_pixel, num_fbs, bounce_buffer_size_px, dma_burst_size;
    int hsync_gpio_num, vsync_gpio_num, de_gpio_num, pclk_gpio_num, disp_gpio_num;
    int data_gpio_nums[16];
    struct { bool fb_in_psram; } flags;
} esp_lcd_rgb_panel_config_t;
esp_err_t esp_lcd_new_rgb_panel(const esp_lcd_rgb_panel_config_t *config, esp_lcd_panel_handle_t *panel);
esp_err_t esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t panel, unsigned count, ...);
esp_err_t esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_callbacks_t *callbacks, void *context);
