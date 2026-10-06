#pragma once
void core_boot_log(const char *tag, const char *format, ...);
#define ESP_LOGI(...) core_boot_log(__VA_ARGS__)
#define ESP_LOGW(...) core_boot_log(__VA_ARGS__)
#define ESP_LOGE(...) core_boot_log(__VA_ARGS__)
