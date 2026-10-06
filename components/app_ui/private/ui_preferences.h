#pragma once
#include "app_message.h"
/* Boot-loaded persistent values. Normal application never writes storage or
 * changes these values; maintenance saves settings for the next boot. */
esp_err_t ui_preferences_start(void);
bool ui_preferences_quiesce(uint32_t timeout_ms);
unsigned ui_preferences_level(void);
unsigned ui_preferences_pad(void);
esp_err_t ui_preferences_message(const app_message_t *message,app_message_t *reply,bool *deferred);
