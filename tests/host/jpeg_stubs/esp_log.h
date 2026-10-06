#pragma once
void fake_log(const char *,const char *,...);
#define ESP_LOGI fake_log
#define ESP_LOGW fake_log
#define ESP_LOGE fake_log
#define ESP_LOGD fake_log
