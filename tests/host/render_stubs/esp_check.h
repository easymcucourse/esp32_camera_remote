#pragma once
#include "esp_err.h"
#include "esp_log.h"
#define ESP_RETURN_ON_FALSE(c,e,...) do { if(!(c)) return(e); } while(0)
#define ESP_RETURN_ON_ERROR(c,...) do { esp_err_t result_=(c);if(result_!=ESP_OK)return result_; } while(0)
