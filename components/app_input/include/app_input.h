#pragma once
#include "esp_err.h"
#include <stdint.h>
#define APP_INPUT_API_VERSION 1
/* Core-only lifecycle. Providers register/publish values through input_provider.
 * One 4096-byte internal-RAM priority4 owner consumes reports and typed messages.
 * Quiesce closes admission, completes safety handoffs, then stops its endpoint.
 * Camera's physical release/JPEG drain remains Core's subsequent Camera STOP.
 * Provider transport tasks are stopped separately by their composition owner. */
/* Core serializes these calls outside the Input task. Duplicate start returns
 * INVALID_STATE; failed start unwinds its endpoint/registry allocations.
 * Restart requires every previous provider registration to be returned.
 * Quiesce is idempotent after join; timeout keeps admission closed and retains
 * the stopping task/endpoint for a later quiesce retry, without forced deletion. */
esp_err_t app_input_start(void);
esp_err_t app_input_quiesce(uint32_t timeout_ms);
