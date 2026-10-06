#pragma once
#include "app_maintenance_web.h"
esp_err_t app_core_settings_read(app_maintenance_settings_t *settings);
esp_err_t app_core_settings_write(const app_maintenance_settings_t *settings);
