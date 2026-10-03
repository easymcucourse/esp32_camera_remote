#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef bool (*debug_command_t)(int argc, char **argv);
/* One UART0 consumer per firmware, internal stack / priority 2. Common commands
 * are handled first, then dispatch. Neither dispatch nor poll may render inline. */
esp_err_t debug_console_start(debug_command_t dispatch, void (*poll)(void));
/* Console dispatch/poll only. Preserves an optional #request prefix in replies. */
int debug_printf(const char *format, ...);
/* Shared by every async producer on this UART; nonzero IDs cannot collide
 * between a pad job, a UI preference, a Wi-Fi update and a raw request. */
uint32_t debug_async_token(void);
