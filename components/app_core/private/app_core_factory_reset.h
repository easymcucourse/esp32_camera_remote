#pragma once
#include "app_wifi.h"
#include "esp_err.h"
/* Internal-RAM caller; exclusively stopped normal writers required. Freezes
 * the sole config worker before taking a fresh persistent Wi-Fi snapshot.
 * Successful reset retains the freeze until reboot; failure resumes only
 * maintenance configuration admission, never normal application owners. */
esp_err_t app_core_factory_reset(app_wifi_t *wifi,bool all);
