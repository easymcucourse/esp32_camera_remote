#include "ble_clients.h"
#include <stdatomic.h>
static atomic_uint scan_owner;
static atomic_bool scan_ready;
static void gap(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p)
{
    if (event==ESP_GAP_BLE_SEC_REQ_EVT) {
        /* Each client only answers its own peer's security request. */
        ble_gamepad_gap_event(event,p); gimbal_gap_event(event,p); return;
    }
    if (event==ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT)
        scan_ready=p->scan_param_cmpl.status==ESP_BT_STATUS_SUCCESS;
    bool scan=event==ESP_GAP_BLE_SCAN_RESULT_EVT || event==ESP_GAP_BLE_SCAN_START_COMPLETE_EVT;
    unsigned owner=atomic_load(&scan_owner);
    if (!scan || owner==1) ble_gamepad_gap_event(event,p);
    if (!scan || owner==2) gimbal_gap_event(event,p);
    if ((event==ESP_GAP_BLE_SCAN_START_COMPLETE_EVT && p->scan_start_cmpl.status!=ESP_BT_STATUS_SUCCESS) ||
        (event==ESP_GAP_BLE_SCAN_RESULT_EVT && p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_CMPL_EVT))
        atomic_store(&scan_owner,0);
}
static void gatt(esp_gattc_cb_event_t event, esp_gatt_if_t iface, esp_ble_gattc_cb_param_t *p)
{
    ble_gamepad_gatt_event(event,iface,p); gimbal_gatt_event(event,iface,p);
}
esp_err_t ble_clients_init(void)
{
    esp_err_t err=esp_ble_gap_register_callback(gap);
    return err==ESP_OK?esp_ble_gattc_register_callback(gatt):err;
}
bool ble_clients_scan_ready(void) { return atomic_load(&scan_ready); }
esp_err_t ble_clients_scan(unsigned owner, unsigned seconds)
{
    unsigned expected=0;
    if (owner<1 || owner>2 || !ble_clients_scan_ready() ||
        !atomic_compare_exchange_strong(&scan_owner,&expected,owner)) return ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_ble_gap_start_scanning(seconds);
    if (err!=ESP_OK) atomic_store(&scan_owner,0);
    return err;
}
