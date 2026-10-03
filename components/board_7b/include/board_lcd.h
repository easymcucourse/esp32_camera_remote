#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Serialized by board_7b's display mutex; initialize the entire provided buffer.
 * During scan recovery this is called only after ownership is confirmed. */
typedef void (*board_lcd_prepare_t)(uint16_t *pixels);
esp_err_t board_lcd_init(board_lcd_prepare_t prepare);
bool board_lcd_ready(void);
uint16_t *board_lcd_back_buffer(void);
/* On any publication failure, no buffer may be written until recovery succeeds. */
esp_err_t board_lcd_publish(uint16_t *pixels);
/* Restart existing RGB/GDMA buffers first; replace the panel on driver errors.
 * Three attempts maximum; failure leaves no writable framebuffer. */
esp_err_t board_lcd_recover(board_lcd_prepare_t prepare);
/* Development only: 0=off, 1=until scan restart, 2=until off or reboot. */
esp_err_t board_lcd_test_fault(unsigned mode);
