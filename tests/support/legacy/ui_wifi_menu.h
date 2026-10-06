#pragma once
#include "app_message.h"
esp_err_t ui_wifi_menu_start(void);
bool ui_wifi_menu_quiesce(uint32_t timeout_ms);
bool ui_wifi_menu_active(void);
esp_err_t ui_wifi_menu_action(const app_message_t *message);
