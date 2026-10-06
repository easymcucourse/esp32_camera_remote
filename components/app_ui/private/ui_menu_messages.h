#pragma once
#include "app_message.h"
void ui_menu_snapshot(app_ui_state_t *state);
esp_err_t ui_menu_message_apply(const app_message_t *message, app_message_t *reply);
esp_err_t ui_property_message(const app_message_t *message,app_message_t *reply);
