#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
/* Nonblocking Core claim: close business admission, preserve safety/completion
 * routing until physical owners return. Does not wait on the HTTP caller. */
void app_core_normal_close(void);
/* Core health or startup failure on its internal-RAM stack. Irreversible; no resume.
 * Failure preserves closed owners and requires restart, never activation. */
bool app_core_normal_stop(void);
