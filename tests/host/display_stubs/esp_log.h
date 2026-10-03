#pragma once
void fake_log(const char *tag, const char *format, ...);
#define ESP_LOGI fake_log
#define ESP_LOGW fake_log
#define ESP_LOGE fake_log
