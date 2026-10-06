#include "app_core_settings.h"
#include "preferences_store.h"
esp_err_t app_core_settings_read(app_maintenance_settings_t *settings) { return preferences_store_load(settings); }
esp_err_t app_core_settings_write(const app_maintenance_settings_t *settings) { return preferences_store_write(settings); }
