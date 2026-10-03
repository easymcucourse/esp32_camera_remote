#include "ble_gamepad.h"
#include "ble_advertisement.h"
#include "ultimate2_report.h"
#include "ds4_host.h"
#include "matrix_status.h"
#include "debug_console.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>
#if !CONFIG_BTDM_CTRL_MODE_BTDM || !CONFIG_BT_BLE_ENABLED || !CONFIG_BT_GATTC_ENABLE
#error "BLE gamepad requires updated dual-mode sdkconfig; regenerate from sdkconfig.defaults"
#endif

static const char *TAG="ble_gamepad";
typedef struct { uint8_t address[6],type;ble_advertisement_t advertisement; } candidate_t;
static candidate_t candidates[8];
static unsigned candidate_count;
static bool candidates_overflow;
static unsigned scan_packets,scan_named;
static portMUX_TYPE scan_lock=portMUX_INITIALIZER_UNLOCKED;
static atomic_bool registered,scan_ready,scan_finished,scanning,linking,connected;
static atomic_bool authenticated,services_ready,discovery_started;
static esp_gatt_if_t client_if=ESP_GATT_IF_NONE;
static atomic_uint connection,battery_handle;
static uint16_t battery_start,battery_end;
static uint16_t hid_start,hid_end;
static atomic_uint map_handle,map_tries,notify_total;
static atomic_bool map_known;
static atomic_llong read_started;
static esp_gattc_char_elem_t input_reports[8];
static atomic_uint input_count;
static uint32_t input_packets;
static int64_t input_log_at;
static uint8_t report_map[512];
static unsigned report_map_size;
static bool hid_found;
static atomic_bool input_supported;
static atomic_llong last_input_at;
static atomic_bool input_live;
static uint8_t peer[6];
static int64_t started;
static atomic_bool battery_read_pending;
static esp_ble_scan_params_t scan_params={.scan_type=BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type=BLE_ADDR_TYPE_PUBLIC,.scan_filter_policy=BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval=0x50,.scan_window=0x30,.scan_duplicate=BLE_SCAN_DUPLICATE_DISABLE};
static bool is_peer(const uint8_t *address)
{
    portENTER_CRITICAL(&scan_lock);bool same=!memcmp(peer,address,6);portEXIT_CRITICAL(&scan_lock);
    return same;
}
bool ble_gamepad_command(int argc,char **argv)
{
    if (strcmp(argv[0],"ble")) return false;
    if (argc!=2 || strcmp(argv[1],"map")) { debug_printf("[dbg] ERR ble map\n");return true; }
    uint8_t copy[sizeof(report_map)];unsigned size;
    portENTER_CRITICAL(&scan_lock);size=report_map_size;memcpy(copy,report_map,size);portEXIT_CRITICAL(&scan_lock);
    debug_printf("[dbg] BLE profile supported=%d notify_count=%u map_handle=%u attempts=%u\n",
        atomic_load(&input_supported),atomic_load(&notify_total),atomic_load(&map_handle),atomic_load(&map_tries));
    if (!atomic_load(&connected) || !size) { debug_printf("[dbg] ERR BLE report map unavailable\n");return true; }
    debug_printf("[dbg] OK ble map bytes=%u\n",size);
    for (unsigned at=0;at<size;at+=24) {
        char hex[49];unsigned n=size-at;if (n>24) n=24;
        for (unsigned i=0;i<n;++i) {
            const char *digits="0123456789abcdef";hex[i*2]=digits[copy[at+i]>>4];hex[i*2+1]=digits[copy[at+i]&15];
        }
        hex[n*2]=0;debug_printf("[dbg] BLE MAP offset=%u data=%s\n",at,hex);
    }
    return true;
}

static void battery(const uint8_t *value,unsigned length)
{
    if (length!=1 || *value>100) {
        matrix_status_set_ble_gamepad_battery(255);
        ESP_LOGW(TAG,"Invalid Battery Level length=%u",length);return;
    }
    matrix_status_set_ble_gamepad_battery(*value);
    ESP_LOGI(TAG,"BLE controller battery=%u%%",*value);
}
static void close_peer(const char *reason)
{
    ESP_LOGW(TAG,"Closing BLE controller: %s",reason);
    esp_err_t err=esp_ble_gattc_close(client_if,connection);
    if (err!=ESP_OK) ESP_LOGW(TAG,"Close failed: %s",esp_err_to_name(err));
}
static void discover(void)
{
    bool expected=false;
    if (!atomic_load(&authenticated) || !atomic_load(&services_ready) ||
        !atomic_compare_exchange_strong(&discovery_started,&expected,true)) return;
    ESP_LOGI(TAG,"BLE authenticated; discovering HID and battery services");
    if (esp_ble_gattc_search_service(client_if,connection,NULL)!=ESP_OK) close_peer("service search could not start");
}
static void gap_event(esp_gap_ble_cb_event_t event,esp_ble_gap_cb_param_t *p)
{
    if (event==ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT) {
        atomic_store(&scan_ready,p->scan_param_cmpl.status==ESP_BT_STATUS_SUCCESS);
    } else if (event==ESP_GAP_BLE_SCAN_START_COMPLETE_EVT) {
        if (p->scan_start_cmpl.status!=ESP_BT_STATUS_SUCCESS) {
            atomic_store(&scanning,false);atomic_store(&scan_finished,true);
        }
    } else if (event==ESP_GAP_BLE_SCAN_RESULT_EVT) {
        if (p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_CMPL_EVT) {
            atomic_store(&scanning,false);atomic_store(&scan_finished,true);
        } else if (p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_RES_EVT) {
            portENTER_CRITICAL(&scan_lock);
            ++scan_packets;
            unsigned i=0;
            while (i<candidate_count && memcmp(candidates[i].address,p->scan_rst.bda,6)) ++i;
            candidate_t item=i<candidate_count?candidates[i]:(candidate_t){0};
            bool valid=ble_advertisement_parse(&item.advertisement,p->scan_rst.ble_adv,p->scan_rst.adv_data_len) &&
                ble_advertisement_parse(&item.advertisement,p->scan_rst.ble_adv+p->scan_rst.adv_data_len,p->scan_rst.scan_rsp_len);
            bool log_name=valid && item.advertisement.name[0] && scan_named<8;
            if (log_name) ++scan_named;
            /* Keep HID/gamepad partial fields to merge later advertising packets. */
            if (valid && (item.advertisement.hid || item.advertisement.gamepad)) {
                if (i<8) {
                    memcpy(item.address,p->scan_rst.bda,6);item.type=p->scan_rst.ble_addr_type;
                    candidates[i]=item;if (i==candidate_count) ++candidate_count;
                } else candidates_overflow=true;
            }
            portEXIT_CRITICAL(&scan_lock);
            if (log_name) ESP_LOGD(TAG,"BLE seen name='%s' HID=%d gamepad=%d",item.advertisement.name,
                                  item.advertisement.hid,item.advertisement.gamepad);
        }
    } else if (event==ESP_GAP_BLE_SEC_REQ_EVT) {
        esp_ble_gap_security_rsp(p->ble_security.ble_req.bd_addr,
            atomic_load(&linking) && is_peer(p->ble_security.ble_req.bd_addr));
    } else if (event==ESP_GAP_BLE_AUTH_CMPL_EVT && atomic_load(&linking) && is_peer(p->ble_security.auth_cmpl.bd_addr)) {
        if (!p->ble_security.auth_cmpl.success) close_peer("pairing failed");
        else {
            atomic_store(&authenticated,true);discover();
        }
    }
}
static void gatt_event(esp_gattc_cb_event_t event,esp_gatt_if_t iface,esp_ble_gattc_cb_param_t *p)
{
    if (event==ESP_GATTC_REG_EVT) {
        if (p->reg.status==ESP_GATT_OK) {
            client_if=iface;atomic_store(&registered,true);
            esp_ble_gap_set_scan_params(&scan_params);
        }
        return;
    }
    if (iface!=client_if) return;
    switch (event) {
    case ESP_GATTC_OPEN_EVT:
        if (p->open.status!=ESP_GATT_OK) {
            atomic_store(&linking,false);matrix_status_set_ble_gamepad(MATRIX_DISCONNECTED);
            ESP_LOGW(TAG,"BLE open failed status=%u",p->open.status);break;
        }
        connection=p->open.conn_id;battery_handle=battery_start=battery_end=0;
        hid_start=hid_end=0;map_handle=map_tries=notify_total=0;map_known=false;read_started=0;input_count=input_packets=0;input_log_at=0;
        portENTER_CRITICAL(&scan_lock);report_map_size=0;portEXIT_CRITICAL(&scan_lock);
        hid_found=false;input_supported=false;input_live=false;last_input_at=0;battery_read_pending=false;
        atomic_store(&authenticated,false);atomic_store(&services_ready,false);atomic_store(&discovery_started,false);
        if (esp_ble_set_encryption(p->open.remote_bda,ESP_BLE_SEC_ENCRYPT)!=ESP_OK) close_peer("encryption could not start");
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (p->dis_srvc_cmpl.status!=ESP_GATT_OK) close_peer("service discovery failed");
        else { atomic_store(&services_ready,true);discover(); }
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        if (p->search_res.srvc_id.uuid.len==ESP_UUID_LEN_16) {
            unsigned uuid=p->search_res.srvc_id.uuid.uuid.uuid16;
            if (uuid==0x1812) { hid_found=true;hid_start=p->search_res.start_handle;hid_end=p->search_res.end_handle; }
            if (uuid==0x180f) { battery_start=p->search_res.start_handle;battery_end=p->search_res.end_handle; }
        }
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT: {
        if (p->search_cmpl.status!=ESP_GATT_OK || !hid_found) { close_peer("no verified HID service");break; }
        /* Connected here denotes verified BLE HID transport; input reports are
         * not yet mapped into the LCD input source. */
        if (battery_start) {
            esp_bt_uuid_t uuid={.len=ESP_UUID_LEN_16,.uuid.uuid16=0x2a19};
            esp_gattc_char_elem_t element;uint16_t count=1;
            if (esp_ble_gattc_get_char_by_uuid(iface,connection,battery_start,battery_end,uuid,&element,&count)==ESP_GATT_OK &&
                count && (element.properties&ESP_GATT_CHAR_PROP_BIT_READ)) battery_handle=element.char_handle;
        }
        matrix_status_set_ble_gamepad(MATRIX_CONNECTED);atomic_store(&connected,true);
        esp_bt_uuid_t map_uuid={.len=ESP_UUID_LEN_16,.uuid.uuid16=0x2a4b};
        esp_gattc_char_elem_t map;uint16_t maps=1;
        if (esp_ble_gattc_get_char_by_uuid(iface,connection,hid_start,hid_end,map_uuid,&map,&maps)==ESP_GATT_OK && maps) {
            map_handle=map.char_handle; /* Worker serializes reads after notify setup. */
        }
        esp_bt_uuid_t report_uuid={.len=ESP_UUID_LEN_16,.uuid.uuid16=0x2a4d};uint16_t reports=8;
        if (esp_ble_gattc_get_char_by_uuid(iface,connection,hid_start,hid_end,report_uuid,input_reports,&reports)==ESP_GATT_OK) {
            input_count=reports;
            notify_total=0;
            for (unsigned i=0;i<input_count;++i) if (input_reports[i].properties&ESP_GATT_CHAR_PROP_BIT_NOTIFY) ++notify_total;
            for (unsigned i=0;i<input_count;++i) if (input_reports[i].properties&ESP_GATT_CHAR_PROP_BIT_NOTIFY) {
                esp_err_t err=esp_ble_gattc_register_for_notify(iface,peer,input_reports[i].char_handle);
                if (err!=ESP_OK) ESP_LOGW(TAG,"HID notify registration failed: %s",esp_err_to_name(err));
            }
        }
        if (!battery_handle) ESP_LOGW(TAG,"BLE HID connected without readable Battery Level; row remains unknown");
        else ESP_LOGI(TAG,"BLE HID battery service ready");
        break;
    }
    case ESP_GATTC_READ_CHAR_EVT:
        if (p->read.conn_id!=connection || !atomic_load(&connected)) break;
        if (p->read.handle==map_handle) {
            battery_read_pending=false;
            ESP_LOGI(TAG,"HID report map status=%u bytes=%u",p->read.status,p->read.value_len);
            portENTER_CRITICAL(&scan_lock);
            map_known=p->read.status==ESP_GATT_OK && p->read.value_len>0;
            report_map_size=p->read.status==ESP_GATT_OK && p->read.value_len<=sizeof(report_map)?p->read.value_len:0;
            if (report_map_size) memcpy(report_map,p->read.value,report_map_size);
            portEXIT_CRITICAL(&scan_lock);
            input_supported=ultimate2_map_matches(p->read.value,p->read.status==ESP_GATT_OK?p->read.value_len:0);
            unsigned notify_count=0;
            for (unsigned i=0;i<input_count;++i) if (input_reports[i].properties&ESP_GATT_CHAR_PROP_BIT_NOTIFY) ++notify_count;
            input_supported=input_supported && notify_count==1;
            ESP_LOGI(TAG,"Ultimate 2 input profile supported=%d notify_count=%u",input_supported,notify_count);
            if (p->read.status==ESP_GATT_OK) ESP_LOG_BUFFER_HEX_LEVEL(TAG,p->read.value,p->read.value_len,ESP_LOG_DEBUG);
        } else if (p->read.handle==battery_handle) {
            battery_read_pending=false;
            if (p->read.status==ESP_GATT_OK) battery(p->read.value,p->read.value_len);
            else { matrix_status_set_ble_gamepad_battery(255);ESP_LOGW(TAG,"Battery read failed status=%u",p->read.status); }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (p->reg_for_notify.status!=ESP_GATT_OK) { ESP_LOGW(TAG,"HID notify registration status=%u",p->reg_for_notify.status);break; }
        esp_bt_uuid_t uuid={.len=ESP_UUID_LEN_16,.uuid.uuid16=0x2902};
        esp_gattc_descr_elem_t descriptor;uint16_t count=1;
        if (esp_ble_gattc_get_descr_by_char_handle(iface,connection,p->reg_for_notify.handle,uuid,&descriptor,&count)==ESP_GATT_OK && count) {
            uint8_t enable[2]={1,0};
            if (esp_ble_gattc_write_char_descr(iface,connection,descriptor.handle,sizeof(enable),enable,
                ESP_GATT_WRITE_TYPE_RSP,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK) ESP_LOGW(TAG,"HID notify enable failed");
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
        bool input=false;
        for (unsigned i=0;i<input_count;++i) if (input_reports[i].char_handle==p->notify.handle) input=true;
        if (!input || p->notify.conn_id!=connection || !atomic_load(&connected)) break;
        ds4_state_t snapshot;
        if (input_supported && ultimate2_parse_report(p->notify.value,p->notify.value_len,&snapshot)) {
            last_input_at=esp_timer_get_time();input_live=true;ds4_host_apply_ble(&snapshot);
        }
        ++input_packets;int64_t now=esp_timer_get_time();
        if (now-input_log_at>=1000000 || input_packets<=3) {
            input_log_at=now;
            ESP_LOGD(TAG,"HID input handle=%u bytes=%u packets=%lu",p->notify.handle,p->notify.value_len,(unsigned long)input_packets);
            ESP_LOG_BUFFER_HEX_LEVEL(TAG,p->notify.value,p->notify.value_len,ESP_LOG_DEBUG);
        }
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGI(TAG,"HID notify descriptor status=%u",p->write.status);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        atomic_store(&connected,false);atomic_store(&linking,false);
        input_live=false;input_supported=false;
        ds4_host_apply_ble(&(ds4_state_t){.battery=255});
        matrix_status_set_ble_gamepad(MATRIX_DISCONNECTED);
        portENTER_CRITICAL(&scan_lock);report_map_size=0;portEXIT_CRITICAL(&scan_lock);
        ESP_LOGI(TAG,"BLE disconnected reason=0x%02x; battery cleared",p->disconnect.reason);
        break;
    default:break;
    }
}
static void worker(void *arg)
{
    (void)arg;int64_t next_scan=0,next_read=0;
    while (true) {
        int64_t now=esp_timer_get_time();
        if (atomic_exchange(&input_live,false)) {
            /* Restore true below only while the timestamp remains fresh. */
            if (now-atomic_load(&last_input_at)>=1000000)
                ds4_host_apply_ble(&(ds4_state_t){.battery=255});
            else atomic_store(&input_live,true);
        }
        if (atomic_load(&connected)) {
            if (battery_read_pending && now-atomic_load(&read_started)>=10000000) battery_read_pending=false;
            if (map_handle && !map_known && map_tries<3 && !battery_read_pending && now>=next_read) {
                battery_read_pending=true;read_started=now;next_read=now+1000000;++map_tries;
                if (esp_ble_gattc_read_char(client_if,connection,map_handle,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK) battery_read_pending=false;
            } else if (battery_handle && !battery_read_pending && now>=next_read) {
                battery_read_pending=true;read_started=now;next_read=now+10000000;
                if (esp_ble_gattc_read_char(client_if,connection,battery_handle,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK) battery_read_pending=false;
            }
        } else if (atomic_load(&linking)) {
            if (now-started>=20000000) { close_peer("setup timeout");started=now; }
        } else if (atomic_exchange(&scan_finished,false)) {
            candidate_t chosen={0};unsigned found=0;
            portENTER_CRITICAL(&scan_lock);
            bool overflow=candidates_overflow;unsigned packets=scan_packets;
            /* Some controllers omit 0x1812 from advertising. Gamepad appearance
             * or a supported name selects a candidate; GATT must still prove HID. */
            for (unsigned i=0;i<candidate_count;++i) if (candidates[i].advertisement.gamepad) {
                chosen=candidates[i];++found;
            }
            portEXIT_CRITICAL(&scan_lock);
            if (found==1 && !overflow) {
                portENTER_CRITICAL(&scan_lock);memcpy(peer,chosen.address,6);portEXIT_CRITICAL(&scan_lock);
                atomic_store(&linking,true);started=now;
                matrix_status_set_ble_gamepad(MATRIX_CONNECTING);
                ESP_LOGI(TAG,"Connecting BLE gamepad '%s'",chosen.advertisement.name);
                if (esp_ble_gattc_open(client_if,peer,chosen.type,true)!=ESP_OK) {
                    atomic_store(&linking,false);matrix_status_set_ble_gamepad(MATRIX_DISCONNECTED);
                }
            } else ESP_LOGI(TAG,"BLE scan packets=%u gamepads=%u overflow=%d; waiting for one pairing device",packets,found,overflow);
            next_scan=now+3000000;
        } else if (atomic_load(&registered) && atomic_load(&scan_ready) && !atomic_load(&scanning) && now>=next_scan) {
            portENTER_CRITICAL(&scan_lock);candidate_count=scan_packets=scan_named=0;candidates_overflow=false;portEXIT_CRITICAL(&scan_lock);
            atomic_store(&scanning,true);
            if (esp_ble_gap_start_scanning(5)!=ESP_OK) atomic_store(&scanning,false);
            next_scan=now+8000000;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
esp_err_t ble_gamepad_init(void)
{
    esp_err_t err=esp_ble_gap_register_callback(gap_event);if (err!=ESP_OK) return err;
    err=esp_ble_gattc_register_callback(gatt_event);if (err!=ESP_OK) return err;
    uint8_t auth=ESP_LE_AUTH_REQ_SC_BOND,io=ESP_IO_CAP_NONE,key_size=16;
    err=esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE,&auth,sizeof(auth));if (err!=ESP_OK) return err;
    err=esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE,&io,sizeof(io));if (err!=ESP_OK) return err;
    err=esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE,&key_size,sizeof(key_size));if (err!=ESP_OK) return err;
    err=esp_ble_gattc_app_register(0x42);if (err!=ESP_OK) return err;
    return xTaskCreate(worker,"ble_pad",4096,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
