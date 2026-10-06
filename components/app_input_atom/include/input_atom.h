#pragma once
#include "esp_err.h"
#include <stdint.h>
#define APP_INPUT_ATOM_API_VERSION 1
/* Core-only physical provider lifecycle; I2C bus remains board-owned. */
/* Startup caller prepares the I2C device before AP startup, without a task,
 * endpoint or Input registration. Idempotent until stop; errors are synchronous.
 * start consumes this preparation (or prepares for standalone callers).
 * stop also returns an unused prepared device; failed removal retains ownership. */
/* Lifecycle calls are serialized by Core and must not run on the provider task.
 * Duplicate start returns INVALID_STATE. Failed start retains any prepared I2C
 * device for retry or stop; endpoint/provider registration is unwound.
 * Stop is idempotent after release. Timeout closes admission and retains the
 * running owner/device; retry stop, never start over that owner. */
esp_err_t input_atom_prepare(void);
esp_err_t input_atom_start(void);
esp_err_t input_atom_stop(uint32_t timeout_ms);
