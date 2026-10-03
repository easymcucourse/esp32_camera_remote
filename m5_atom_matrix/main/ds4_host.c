#include "ds4_host.h"
#include "atom_protocol.h"
#include "matrix_status.h"
#include <string.h>
#include <stdlib.h>
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_hidh_api.h"
#include "esp_log.h"
#include "esp_check.h"
#include "esp_random.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ds4_host";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static ds4_state_t state;
static ds4_state_t real_state;
static bool sim_active;
static uint8_t input_generation;
static ds4_events_t events;
static bool ready, scanning, connecting, active, approved, have_saved;
static bool have_candidate, save_pending;
static uint8_t saved[6], target[6], candidate[6];
static uint8_t active_handle;
static TickType_t opened_at;
static nvs_handle_t storage;
static uint32_t input_reports, rejected_reports;
static TickType_t last_input_log;

/* Both sources pass through exactly the same publication and event rules. */
static void apply_locked(bool simulated, const ds4_state_t *next)
{
    if (simulated != sim_active) return;
    ds4_state_t clean = *next;
    if (!clean.connected) clean = (ds4_state_t){.battery=255};
    clean.buttons &= (1u<<18)-1;
    if (state.connected!=clean.connected) ++input_generation;
    if (state.buttons != clean.buttons) ds4_events_push(&events, clean.buttons);
    state = clean;
}
bool ds4_host_sim_active(void)
{ portENTER_CRITICAL(&lock); bool value=sim_active; portEXIT_CRITICAL(&lock); return value; }
void ds4_host_set_sim(bool enabled)
{
#if CONFIG_REMOTE_DBG_SIM
    portENTER_CRITICAL(&lock);
    /* Retire queued presses. The wire SIM flag makes LCD release the old source;
     * don't mark a gap on the first new press after the neutral connection. */
    events.head=events.count=0; events.last_buttons=0; events.gap_pending=false;
    ++input_generation; sim_active=enabled; state=(ds4_state_t){.battery=255};
    portEXIT_CRITICAL(&lock);
#else
    (void)enabled;
#endif
}
void ds4_host_apply_sim(const ds4_state_t *next)
{
#if CONFIG_REMOTE_DBG_SIM
    portENTER_CRITICAL(&lock); apply_locked(true,next); portEXIT_CRITICAL(&lock);
#else
    (void)next;
#endif
}
void ds4_host_sim_overflow(void)
{
#if CONFIG_REMOTE_DBG_SIM
    portENTER_CRITICAL(&lock);
    if (sim_active) { ++events.dropped; events.gap_pending=true; }
    portEXIT_CRITICAL(&lock);
#endif
}

static const char *connection_name(esp_hidh_connection_state_t value)
{
    switch (value) {
    case ESP_HIDH_CONN_STATE_CONNECTED: return "connected";
    case ESP_HIDH_CONN_STATE_CONNECTING: return "connecting";
    case ESP_HIDH_CONN_STATE_DISCONNECTED: return "disconnected";
    case ESP_HIDH_CONN_STATE_DISCONNECTING: return "disconnecting";
    default: return "unknown";
    }
}

static void disconnect_peer(uint8_t *address, const char *reason)
{
    ESP_LOGW(TAG, "Disconnect " ESP_BD_ADDR_STR ": %s", ESP_BD_ADDR_HEX(address), reason);
    esp_err_t err = esp_bt_hid_host_disconnect(address);
    if (err != ESP_OK) ESP_LOGW(TAG, "Disconnect API failed: %s", esp_err_to_name(err));
}

void ds4_host_get_state(ds4_state_t *out)
{
    portENTER_CRITICAL(&lock);
    *out = state;
    portEXIT_CRITICAL(&lock);
}

bool ds4_host_read_event(uint32_t ack_id, ds4_event_t *event, uint32_t *dropped)
{
    portENTER_CRITICAL(&lock);
    bool available = ds4_events_read(&events, ack_id, event);
    *dropped = events.dropped;
    portEXIT_CRITICAL(&lock);
    return available;
}

static uint8_t link_state_locked(void)
{
    if (sim_active) return state.connected ? 3 : 0;
    return !ready ? 0 : active && approved && state.connected ? 3 :
        connecting || active ? 2 : 1;
}
void ds4_host_status(uint8_t *link_state, uint32_t *dropped)
{
    portENTER_CRITICAL(&lock);
    *link_state = link_state_locked(); *dropped = events.dropped;
    portEXIT_CRITICAL(&lock);
}
void ds4_host_debug_status(ds4_state_t *snapshot, uint8_t *link, unsigned *queued, uint32_t *dropped)
{
    portENTER_CRITICAL(&lock);
    *snapshot = state; *link = link_state_locked(); *queued = events.count; *dropped = events.dropped;
    portEXIT_CRITICAL(&lock);
}
bool ds4_host_poll(uint32_t ack_id, ds4_state_t *snapshot, uint8_t *link_state,
                   ds4_event_t *event, uint8_t *remaining, uint32_t *dropped, uint8_t *source_tag)
{
    portENTER_CRITICAL(&lock);
    *snapshot = state;
    *link_state = link_state_locked();
    *source_tag = (uint8_t)(input_generation<<1) | (sim_active?ATOM_DEBUG_SIM:0);
    bool available = ds4_events_read(&events, ack_id, event);
    *remaining = available ? (uint8_t)(events.count - 1) : 0;
    *dropped = events.dropped;
    portEXIT_CRITICAL(&lock);
    return available;
}

static bool allowed(const uint8_t *address)
{
    portENTER_CRITICAL(&lock);
    bool busy = connecting || active;
    bool result = (connecting || active) ? memcmp(target, address, 6) == 0 :
                  (have_saved && memcmp(saved, address, 6) == 0);
    portEXIT_CRITICAL(&lock);
    /* Recover an existing Bluetooth bond if an older firmware paired the DS4
     * before our application saved its peer. Identity is still checked by SDP. */
    if (!result && !busy) {
        int count = esp_bt_gap_get_bond_device_num();
        esp_bd_addr_t *bonds = count > 0 ? calloc(count, sizeof(esp_bd_addr_t)) : NULL;
        if (bonds && esp_bt_gap_get_bond_device_list(&count, bonds) == ESP_OK) {
            for (int i = 0; i < count; ++i)
                if (memcmp(address, bonds[i], 6) == 0) result = true;
        }
        free(bonds);
        if (result) ESP_LOGD(TAG, "Accept existing bonded peer " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(address));
    }
    return result;
}

static void gap_event(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    if (event == ESP_BT_GAP_DISC_RES_EVT) {
        char name[64] = {0};
        uint32_t cod = 0;
        for (int i = 0; i < param->disc_res.num_prop; ++i) {
            esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];
            if (prop->type == ESP_BT_GAP_DEV_PROP_COD && prop->len >= sizeof(cod))
                memcpy(&cod, prop->val, sizeof(cod));
            else if (prop->type == ESP_BT_GAP_DEV_PROP_BDNAME) {
                size_t len = prop->len < sizeof(name) - 1 ? prop->len : sizeof(name) - 1;
                memcpy(name, prop->val, len);
            } else if (prop->type == ESP_BT_GAP_DEV_PROP_EIR && name[0] == 0) {
                uint8_t len = 0;
                uint8_t *value = esp_bt_gap_resolve_eir_data(prop->val, ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
                if (!value) value = esp_bt_gap_resolve_eir_data(prop->val, ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
                if (value) memcpy(name, value, len < sizeof(name) - 1 ? len : sizeof(name) - 1);
            }
        }
        /* DS4 advertises as a peripheral/gamepad, named Wireless Controller.
         * SDP vendor/product is checked after opening to reject DS5/other HID. */
        ESP_LOGD(TAG, "Discovery " ESP_BD_ADDR_STR " name='%s' CoD=0x%06lx",
                 ESP_BD_ADDR_HEX(param->disc_res.bda), name, (unsigned long)cod);
        if (strcmp(name, "Wireless Controller") == 0 && ((cod >> 8) & 0x1f) == 5 &&
            (cod & 0x3c) == 0x08) {
            portENTER_CRITICAL(&lock);
            if (scanning && !have_candidate) {
                memcpy(candidate, param->disc_res.bda, 6);
                have_candidate = true;
            }
            portEXIT_CRITICAL(&lock);
            ESP_LOGD(TAG, "DS4 candidate " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(param->disc_res.bda));
        }
    } else if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT) {
        portENTER_CRITICAL(&lock);
        scanning = param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED;
        portEXIT_CRITICAL(&lock);
        ESP_LOGD(TAG, "Discovery state=%s", param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED ? "started" : "stopped");
    } else if (event == ESP_BT_GAP_CFM_REQ_EVT) {
        bool accept = allowed(param->cfm_req.bda);
        ESP_LOGD(TAG, "SSP confirm " ESP_BD_ADDR_STR " accept=%d", ESP_BD_ADDR_HEX(param->cfm_req.bda), accept);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, accept);
    } else if (event == ESP_BT_GAP_PIN_REQ_EVT) {
        esp_bt_pin_code_t pin = {'0', '0', '0', '0'};
        esp_bt_gap_pin_reply(param->pin_req.bda,
                            allowed(param->pin_req.bda) && !param->pin_req.min_16_digit, 4, pin);
    } else if (event == ESP_BT_GAP_AUTH_CMPL_EVT) {
        ESP_LOGI(TAG, "Pairing authentication " ESP_BD_ADDR_STR " status=%d",
                 ESP_BD_ADDR_HEX(param->auth_cmpl.bda), param->auth_cmpl.stat);
    }
}

static void hid_event(esp_hidh_cb_event_t event, esp_hidh_cb_param_t *param)
{
    if (event == ESP_HIDH_INIT_EVT) {
        portENTER_CRITICAL(&lock);
        ready = param->init.status == ESP_HIDH_OK;
        portEXIT_CRITICAL(&lock);
        matrix_status_hid_result(param->init.status == ESP_HIDH_OK);
        ESP_LOGI(TAG, "HID host init status=%d; hold SHARE + PS to pair", param->init.status);
    } else if (event == ESP_HIDH_OPEN_EVT) {
        ESP_LOGI(TAG, "HID open " ESP_BD_ADDR_STR " status=%d phase=%s handle=%u outgoing=%d",
                 ESP_BD_ADDR_HEX(param->open.bd_addr), param->open.status,
                 connection_name(param->open.conn_status), param->open.handle, param->open.is_orig);
        if (!allowed(param->open.bd_addr)) {
            if (param->open.status == ESP_HIDH_OK &&
                param->open.conn_status == ESP_HIDH_CONN_STATE_CONNECTED)
                disconnect_peer(param->open.bd_addr, "address is not the selected/saved DS4");
            else ESP_LOGD(TAG, "Ignoring open event for unselected address");
            return;
        }
        /* HIDH OPEN is also emitted as a progress notification, with status OK
         * and phase CONNECTING. Keep the selected peer until the terminal event;
         * clearing connecting here would reject the real CONNECTED event. */
        if (param->open.status == ESP_HIDH_OK &&
            param->open.conn_status == ESP_HIDH_CONN_STATE_CONNECTING) {
            portENTER_CRITICAL(&lock);
            memcpy(target, param->open.bd_addr, 6);
            connecting = true;
            portEXIT_CRITICAL(&lock);
            ESP_LOGD(TAG, "Connection pending; retaining selected peer");
            return;
        }
        portENTER_CRITICAL(&lock);
        connecting = false;
        active = param->open.status == ESP_HIDH_OK &&
                 param->open.conn_status == ESP_HIDH_CONN_STATE_CONNECTED;
        approved = false;
        memset(&real_state, 0, sizeof(real_state));
        apply_locked(false,&real_state);
        if (active) {
            memcpy(target, param->open.bd_addr, 6);
            active_handle = param->open.handle;
            opened_at = xTaskGetTickCount();
            input_reports = rejected_reports = 0;
            last_input_log = 0;
        }
        portEXIT_CRITICAL(&lock);
    } else if (event == ESP_HIDH_GET_DSCP_EVT) {
        bool is_ds4 = param->dscp.status == ESP_HIDH_OK && param->dscp.vendor_id == 0x054c &&
                      (param->dscp.product_id == 0x05c4 || param->dscp.product_id == 0x09cc);
        uint8_t address[6];
        portENTER_CRITICAL(&lock);
        bool current = active && param->dscp.handle == active_handle;
        if (current) approved = is_ds4;
        memcpy(address, target, 6);
        portEXIT_CRITICAL(&lock);
        ESP_LOGI(TAG, "HID descriptor status=%d handle=%u VID=%04x PID=%04x bytes=%u current=%d approved=%d",
                 param->dscp.status, param->dscp.handle, param->dscp.vendor_id,
                 param->dscp.product_id, param->dscp.dl_len, current, is_ds4);
        if (current && !is_ds4) {
            ESP_LOGW(TAG, "Reject non-DS4 VID=%04x PID=%04x", param->dscp.vendor_id, param->dscp.product_id);
            disconnect_peer(address, "unsupported HID identity");
        } else if (current) {
            /* Reading calibration switches genuine DS4s to full BT report 0x11.
             * Short report 0x01 is also supported until the switch completes. */
            esp_err_t err = esp_bt_hid_host_get_report(address, ESP_HIDH_REPORT_TYPE_FEATURE, 0x02, 37);
            ESP_LOGD(TAG, "Request calibration report 0x02: %s", esp_err_to_name(err));
        }
    } else if (event == ESP_HIDH_DATA_IND_EVT) {
        ds4_state_t next;
        bool parsed = param->data_ind.status == ESP_HIDH_OK &&
                      ds4_parse_report(param->data_ind.data, param->data_ind.len, &next);
        portENTER_CRITICAL(&lock);
        bool current = active && approved && param->data_ind.handle == active_handle;
        bool first = current && parsed && !real_state.connected;
        ++input_reports;
        if (!parsed || !current) ++rejected_reports;
        uint32_t count = input_reports, rejected = rejected_reports;
        if (current && parsed) {
            real_state = next;
            apply_locked(false,&next);
            if (first) save_pending = true;
        }
        portEXIT_CRITICAL(&lock);
        TickType_t now = xTaskGetTickCount();
        if (count <= 5 || now - last_input_log >= pdMS_TO_TICKS(10000)) {
            last_input_log = now;
            unsigned id = param->data_ind.len && param->data_ind.data ? param->data_ind.data[0] : 0;
            ESP_LOGD(TAG, "Input status=%d handle=%u report=0x%02x len=%u parsed=%d current=%d total=%lu rejected=%lu",
                     param->data_ind.status, param->data_ind.handle, id, param->data_ind.len,
                     parsed, current, (unsigned long)count, (unsigned long)rejected);
            if (param->data_ind.data && param->data_ind.len)
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->data_ind.data,
                    param->data_ind.len < 16 ? param->data_ind.len : 16, ESP_LOG_DEBUG);
        }
        if (first) ESP_LOGI(TAG, "DualShock 4 connected; input ready");
    } else if (event == ESP_HIDH_GET_RPT_EVT) {
        ESP_LOGD(TAG, "Feature response status=%d handle=%u bytes=%u", param->get_rpt.status,
                 param->get_rpt.handle, param->get_rpt.len);
    } else if (event == ESP_HIDH_CLOSE_EVT || event == ESP_HIDH_VC_UNPLUG_EVT) {
        uint8_t handle = event == ESP_HIDH_CLOSE_EVT ? param->close.handle : param->unplug.handle;
        esp_hidh_connection_state_t phase = event == ESP_HIDH_CLOSE_EVT ? param->close.conn_status : param->unplug.conn_status;
        ESP_LOGI(TAG, "HID close event=%d status=%d phase=%s handle=%u reason=0x%02x",
                 event, event == ESP_HIDH_CLOSE_EVT ? param->close.status : param->unplug.status,
                 connection_name(phase), handle, event == ESP_HIDH_CLOSE_EVT ? param->close.reason : 0);
        if (phase != ESP_HIDH_CONN_STATE_DISCONNECTED) return;
        portENTER_CRITICAL(&lock);
        bool current = active && handle == active_handle;
        if (current) {
            active = approved = connecting = false;
            save_pending = false;
            memset(&real_state, 0, sizeof(real_state));
            apply_locked(false,&real_state);
        }
        portEXIT_CRITICAL(&lock);
        if (current) ESP_LOGI(TAG, "DualShock 4 disconnected; auto reconnect enabled");
    } else {
        ESP_LOGD(TAG, "HID event=%d", event);
    }
}

static void connection_task(void *arg)
{
    (void)arg;
    TickType_t next_attempt = 0, attempt_start = 0;
    bool try_saved = true, cancel_sent = false;
    while (true) {
        uint8_t address[6];
        portENTER_CRITICAL(&lock);
        bool is_ready = ready, is_active = active, is_connecting = connecting;
        bool is_scanning = scanning, found = have_candidate, known = have_saved;
        bool input_ready = real_state.connected;
        TickType_t open_time = opened_at;
        bool persist = save_pending;
        if (persist) {
            memcpy(address, target, 6);
            save_pending = false;
        }
        portEXIT_CRITICAL(&lock);
        if (persist) {
            esp_err_t err = nvs_set_blob(storage, "peer", address, 6);
            if (err == ESP_OK) err = nvs_commit(storage);
            if (err == ESP_OK) {
                portENTER_CRITICAL(&lock);
                memcpy(saved, address, 6);
                have_saved = true;
                portEXIT_CRITICAL(&lock);
                ESP_LOGI(TAG, "Saved DS4 for automatic reconnect");
            } else ESP_LOGW(TAG, "Save DS4 failed: %s", esp_err_to_name(err));
        }
        TickType_t now = xTaskGetTickCount();
        if (is_scanning && (found || is_active) && !cancel_sent) {
            esp_err_t err = esp_bt_gap_cancel_discovery();
            ESP_LOGD(TAG, "Cancel discovery found=%d active=%d: %s", found, is_active, esp_err_to_name(err));
            if (err == ESP_OK) cancel_sent = true;
        }
        if (!is_scanning) cancel_sent = false;
        if (is_active && !input_ready && now - open_time >= pdMS_TO_TICKS(20000)) {
            portENTER_CRITICAL(&lock);
            memcpy(address, target, 6);
            opened_at = now;
            portEXIT_CRITICAL(&lock);
            disconnect_peer(address, "no valid input within 20 seconds");
            ESP_LOGW(TAG, "No valid DS4 input; disconnecting to retry");
        } else if (is_connecting && now - attempt_start >= pdMS_TO_TICKS(20000)) {
            portENTER_CRITICAL(&lock);
            memcpy(address, target, 6);
            connecting = false;
            portEXIT_CRITICAL(&lock);
            disconnect_peer(address, "connection attempt timed out");
            next_attempt = now + pdMS_TO_TICKS(3000);
            ESP_LOGW(TAG, "Connection timed out; retrying discovery");
        } else if (is_ready && !is_active && !is_connecting && !is_scanning &&
                   (int32_t)(now - next_attempt) >= 0) {
            if (found || (known && try_saved)) {
                portENTER_CRITICAL(&lock);
                memcpy(address, found ? candidate : saved, 6);
                memcpy(target, address, 6);
                have_candidate = false;
                connecting = true;
                portEXIT_CRITICAL(&lock);
                try_saved = false;
                attempt_start = now;
                next_attempt = now + pdMS_TO_TICKS(3000);
                ESP_LOGI(TAG, "Connecting DS4 " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(address));
                ESP_LOGD(TAG, "Connection source=%s", found ? "discovery" : "saved peer");
                esp_err_t err = esp_bt_hid_host_connect(address);
                if (err != ESP_OK) {
                    portENTER_CRITICAL(&lock);
                    connecting = false;
                    portEXIT_CRITICAL(&lock);
                    ESP_LOGW(TAG, "Connect failed: %s", esp_err_to_name(err));
                }
            } else {
                portENTER_CRITICAL(&lock);
                scanning = true;
                have_candidate = false;
                portEXIT_CRITICAL(&lock);
                ESP_LOGI(TAG, "Scanning for DS4; hold SHARE + PS until light flashes");
                esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0);
                if (err != ESP_OK) {
                    portENTER_CRITICAL(&lock);
                    scanning = false;
                    portEXIT_CRITICAL(&lock);
                    ESP_LOGW(TAG, "Discovery failed: %s", esp_err_to_name(err));
                }
                try_saved = true;
                next_attempt = now + pdMS_TO_TICKS(3000);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

esp_err_t ds4_host_init(void)
{
    // Avoid reusing the previous boot's ACK IDs after an ATOM reset.
    events.next_id = esp_random();
#if CONFIG_APP_DS4_DEBUG_LOG
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
#endif
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "erase NVS");
        err = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(err, TAG, "init NVS");
    ESP_RETURN_ON_ERROR(nvs_open("ds4_host", NVS_READWRITE, &storage), TAG, "open pairing storage");
    size_t length = sizeof(saved);
    err = nvs_get_blob(storage, "peer", saved, &length);
    have_saved = err == ESP_OK && length == sizeof(saved);
    if (have_saved) ESP_LOGI(TAG, "Loaded saved DS4 " ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(saved));
    else ESP_LOGI(TAG, "No saved DS4; NVS read=%s bytes=%u", esp_err_to_name(err), (unsigned)length);
    matrix_status_boot_stage(MATRIX_BOOT_BLUETOOTH);
    ESP_RETURN_ON_ERROR(esp_bt_controller_mem_release(ESP_BT_MODE_BLE), TAG, "release BLE");
    esp_bt_controller_config_t config = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_bt_controller_init(&config), TAG, "init controller");
    ESP_RETURN_ON_ERROR(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT), TAG, "enable BT");
    ESP_RETURN_ON_ERROR(esp_bluedroid_init(), TAG, "init Bluedroid");
    ESP_RETURN_ON_ERROR(esp_bluedroid_enable(), TAG, "enable Bluedroid");
    ESP_LOGI(TAG, "Bluetooth bond count=%d", esp_bt_gap_get_bond_device_num());
    ESP_RETURN_ON_ERROR(esp_bt_gap_register_callback(gap_event), TAG, "register GAP");
    esp_bt_io_cap_t capability = ESP_BT_IO_CAP_NONE;
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &capability, sizeof(capability)),
                        TAG, "set pairing capability");
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_device_name("M5ATOM DS4 Host"), TAG, "set name");
    ESP_RETURN_ON_ERROR(esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE), TAG, "set mode");
    ESP_RETURN_ON_ERROR(esp_bt_hid_host_register_callback(hid_event), TAG, "register HID");
    matrix_status_boot_stage(MATRIX_BOOT_HID_HOST);
    ESP_RETURN_ON_ERROR(esp_bt_hid_host_init(), TAG, "init HID host");
    if (xTaskCreate(connection_task, "ds4_connect", 4096, NULL, 4, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    matrix_status_host_task_started();
    return ESP_OK;
}
