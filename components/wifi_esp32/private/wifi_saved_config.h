#pragma once
#include "network_config.h"
#include "esp_err.h"
/* Caller serializes reads/writes. Only the wifi_ap/cfg record is touched. */
esp_err_t wifi_saved_read(network_config_t *config);
esp_err_t wifi_saved_write(const network_config_t *config);
