#pragma once
#include <assert.h>
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGW(tag,...) ((void)(tag))
#define ESP_LOGE(tag,...) ((void)(tag))
#define ESP_ERROR_CHECK(result) assert((result)==ESP_OK)
