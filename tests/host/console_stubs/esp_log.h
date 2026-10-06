#pragma once
typedef int esp_log_level_t;
void esp_log_level_set(const char*,esp_log_level_t);
void fake_log(const char*,const char*,...);
#define ESP_LOGI fake_log
#define ESP_LOGE fake_log
