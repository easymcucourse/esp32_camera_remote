#pragma once
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
/* Single Bluedroid callback registration and scan owner for both clients. */
esp_err_t ble_clients_init(void);
esp_err_t ble_clients_scan(unsigned owner, unsigned seconds);
bool ble_clients_scan_ready(void);
void ble_gamepad_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p);
void ble_gamepad_gatt_event(esp_gattc_cb_event_t event, esp_gatt_if_t iface, esp_ble_gattc_cb_param_t *p);
void gimbal_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p);
void gimbal_gatt_event(esp_gattc_cb_event_t event, esp_gatt_if_t iface, esp_ble_gattc_cb_param_t *p);
