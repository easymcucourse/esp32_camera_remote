#pragma once
#include "app_message.h"
bool app_core_camera_messages_quiesce(uint32_t timeout_ms);
bool app_core_console_ready(void);
esp_err_t app_core_camera_session(const app_message_t *request);
