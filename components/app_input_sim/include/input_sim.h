#pragma once
#include "esp_err.h"
#include <stdint.h>
#define APP_INPUT_SIM_API_VERSION 1
/* Core-only Debug provider lifecycle. No serial parser or business callbacks. */
/* Serialized external-owner calls. Duplicate start returns INVALID_STATE;
 * failed start unwinds endpoint/provider registration. Stop joins the owner
 * after returning outstanding completions; timeout retains the stopping owner
 * for a later stop retry. Stopped stop is idempotent. Release omits this API's
 * implementation; callers must obey CONFIG_REMOTE_DBG_SIM. */
esp_err_t input_sim_start(void);
esp_err_t input_sim_stop(uint32_t timeout_ms);
