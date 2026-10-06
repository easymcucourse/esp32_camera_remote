#pragma once
void factory_test_log(const char *tag, const char *format, ...);
#define ESP_LOGI(...) factory_test_log(__VA_ARGS__)
