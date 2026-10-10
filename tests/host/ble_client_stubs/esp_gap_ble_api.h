#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_STATE, ESP_FAIL, ESP_BT_STATUS_SUCCESS=0 };
typedef enum { ESP_GAP_BLE_SEC_REQ_EVT, ESP_GAP_BLE_AUTH_CMPL_EVT,
    ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT, ESP_GAP_BLE_SCAN_RESULT_EVT,
    ESP_GAP_BLE_SCAN_START_COMPLETE_EVT } esp_gap_ble_cb_event_t;
enum { ESP_GAP_SEARCH_INQ_CMPL_EVT, ESP_GAP_SEARCH_INQ_RES_EVT };
typedef union {
    struct { unsigned status; } scan_param_cmpl, scan_start_cmpl;
    struct { unsigned search_evt; } scan_rst;
} esp_ble_gap_cb_param_t;
typedef void (*esp_gap_ble_cb_t)(esp_gap_ble_cb_event_t,esp_ble_gap_cb_param_t *);
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t cb);
esp_err_t esp_ble_gap_start_scanning(unsigned seconds);
