#pragma once
#include "esp_gap_ble_api.h"
typedef unsigned esp_gatt_if_t;
typedef unsigned esp_gattc_cb_event_t;
typedef struct { unsigned unused; } esp_ble_gattc_cb_param_t;
typedef void (*esp_gattc_cb_t)(esp_gattc_cb_event_t,esp_gatt_if_t,esp_ble_gattc_cb_param_t *);
esp_err_t esp_ble_gattc_register_callback(esp_gattc_cb_t cb);
