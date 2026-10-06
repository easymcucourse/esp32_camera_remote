#pragma once
#include "preferences_config.h"
#include "esp_err.h"
/* Persistent record primitives, no UI/runtime state. Normal UI reads only;
 * Core admits writes after all normal owners drain into exclusive maintenance.
 * Primitives have no private mutex: their composition caller must serialize
 * writes/resets (currently the single HTTP handler task). One NVS implementation.
 * Missing schema is legacy v1, unsupported schemas are rejected/preserved. */
esp_err_t preferences_store_load(preferences_config_t *settings);
esp_err_t preferences_store_write(const preferences_config_t *settings);
esp_err_t preferences_store_reset(void);
