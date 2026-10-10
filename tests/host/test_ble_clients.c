#include "ble_clients.h"
#include <assert.h>
static esp_gap_ble_cb_t gap;
static esp_gattc_cb_t gatt;
static unsigned pad_gap,gimbal_gap,pad_gatt,gimbal_gatt,scans;
static int gap_error,gatt_error,start_error;
esp_err_t esp_ble_gap_register_callback(esp_gap_ble_cb_t cb) { gap=cb; return gap_error; }
esp_err_t esp_ble_gattc_register_callback(esp_gattc_cb_t cb) { gatt=cb; return gatt_error; }
esp_err_t esp_ble_gap_start_scanning(unsigned seconds) { assert(seconds); ++scans; return start_error; }
void ble_gamepad_gap_event(esp_gap_ble_cb_event_t e,esp_ble_gap_cb_param_t *p) { (void)e;(void)p; ++pad_gap; }
void gimbal_gap_event(esp_gap_ble_cb_event_t e,esp_ble_gap_cb_param_t *p) { (void)e;(void)p; ++gimbal_gap; }
void ble_gamepad_gatt_event(esp_gattc_cb_event_t e,esp_gatt_if_t i,esp_ble_gattc_cb_param_t *p) { (void)e;(void)i;(void)p; ++pad_gatt; }
void gimbal_gatt_event(esp_gattc_cb_event_t e,esp_gatt_if_t i,esp_ble_gattc_cb_param_t *p) { (void)e;(void)i;(void)p; ++gimbal_gatt; }
int main(void)
{
    gap_error=ESP_FAIL; assert(ble_clients_init()==ESP_FAIL && !gatt);
    gap_error=0; gatt_error=ESP_FAIL; assert(ble_clients_init()==ESP_FAIL);
    gatt_error=0; assert(ble_clients_init()==ESP_OK);
    assert(ble_clients_scan(1,3)==ESP_ERR_INVALID_STATE && !scans);
    esp_ble_gap_cb_param_t p={.scan_param_cmpl.status=0};
    gap(ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT,&p); assert(ble_clients_scan_ready());
    assert(ble_clients_scan(0,3)==ESP_ERR_INVALID_STATE && ble_clients_scan(3,3)==ESP_ERR_INVALID_STATE);
    start_error=ESP_FAIL; assert(ble_clients_scan(2,3)==ESP_FAIL);
    start_error=0; assert(ble_clients_scan(1,5)==ESP_OK && ble_clients_scan(2,3)==ESP_ERR_INVALID_STATE);
    pad_gap=gimbal_gap=0; p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;
    gap(ESP_GAP_BLE_SCAN_RESULT_EVT,&p); assert(pad_gap==1 && !gimbal_gap);
    p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_CMPL_EVT; gap(ESP_GAP_BLE_SCAN_RESULT_EVT,&p);
    assert(ble_clients_scan(2,3)==ESP_OK);
    pad_gap=gimbal_gap=0; p.scan_rst.search_evt=ESP_GAP_SEARCH_INQ_RES_EVT;
    gap(ESP_GAP_BLE_SCAN_RESULT_EVT,&p); assert(!pad_gap && gimbal_gap==1);
    gap(ESP_GAP_BLE_AUTH_CMPL_EVT,&p); gap(ESP_GAP_BLE_SEC_REQ_EVT,&p);
    assert(pad_gap==2 && gimbal_gap==3);
    p.scan_start_cmpl.status=1; gap(ESP_GAP_BLE_SCAN_START_COMPLETE_EVT,&p);
    assert(ble_clients_scan(1,5)==ESP_OK); /* async start failure releases owner */
    esp_ble_gattc_cb_param_t q={0}; gatt(1,0x43,&q); assert(pad_gatt==1 && gimbal_gatt==1);
    return 0;
}
