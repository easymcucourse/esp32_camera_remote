#pragma once
#include "gamepad_input.h"
void wifi_menu_ui_start(void);
/* Consumes menu actions; copied nonblocking queue, never NVS or display here. */
bool wifi_menu_ui_action(pad_action_t action);
bool wifi_menu_ui_active(void);
