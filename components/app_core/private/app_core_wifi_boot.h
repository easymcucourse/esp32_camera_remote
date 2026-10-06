#pragma once
#include "app_wifi.h"
#include "esp_err.h"
/* Core startup-only object owner. Reads saved config without rewriting an
 * invalid/default record; AP fallback is RAM-only. Keep object through reboot. */
esp_err_t app_core_wifi_create(void);
esp_err_t app_core_wifi_start(void);
void app_core_wifi_config(network_config_t *out);
app_wifi_t *app_core_wifi_service(void);
