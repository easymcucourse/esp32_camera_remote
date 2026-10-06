#pragma once
#include "network_config.h"
typedef enum { WIFI_APPLY_OK, WIFI_APPLY_INVALID, WIFI_APPLY_SAVE_FAILED, WIFI_APPLY_DRIVER_FAILED, WIFI_APPLY_ROLLBACK_FAILED } wifi_apply_result_t;
typedef bool (*wifi_apply_fn)(void *context, const network_config_t *config);
wifi_apply_result_t wifi_apply_config(network_config_t *current, const network_config_t *next,
                                      unsigned max_channel, wifi_apply_fn save, wifi_apply_fn restart, void *context);
