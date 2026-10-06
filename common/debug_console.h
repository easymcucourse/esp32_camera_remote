#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef bool (*debug_command_t)(int argc, char **argv);
/* One UART0 consumer per firmware, internal stack / priority 2. Common commands
 * are handled first, then dispatch. Neither dispatch nor poll may render inline. */
esp_err_t debug_console_start(debug_command_t dispatch, void (*poll)(void));
/* UART owner can retire its endpoint when reader exits (error or stop).
 * Callback runs on the reader; must not wait for this reader or stop business. */
esp_err_t debug_console_start_owner(debug_command_t dispatch,void (*poll)(void),void (*retire)(void));
/* Cooperative stop; timeout retains the worker until driver cleanup succeeds. */
bool debug_console_stop(uint32_t timeout_ms);
/* Console dispatch/poll only. Preserves an optional #request prefix in replies. */
int debug_printf(const char *format, ...);
/* Shared by every async producer on this UART; nonzero IDs cannot collide
 * between a pad job, a UI preference, a Wi-Fi update and a raw request. */
uint32_t debug_async_token(void);
