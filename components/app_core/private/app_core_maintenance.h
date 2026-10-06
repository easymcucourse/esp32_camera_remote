#pragma once
#include "esp_err.h"
#include <stdbool.h>
typedef struct app_wifi app_wifi_t;
esp_err_t app_core_maintenance_init(app_wifi_t *wifi);
/* Existing health task only; drains normal services, switches fixed LCD and
 * restarts only the config worker before publishing full maintenance routes. */
void app_core_maintenance_poll(void);
/* System UI permission: closes the physical listener before returning success.
 * Stop failure enters RESTART, never reopens the startup window. */
esp_err_t app_core_enter_normal(void);
bool app_core_maintenance_stop(unsigned timeout_ms);
