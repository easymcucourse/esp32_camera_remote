#pragma once
#include "app_message.h"
esp_err_t ui_bench_message(const app_message_t *message,app_message_t *reply);
bool ui_bench_quiesce(uint32_t timeout_ms);
