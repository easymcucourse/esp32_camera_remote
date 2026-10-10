#include "gimbal_link.h"
#include "ble_clients.h"
#include "ble_advertisement.h"
#include "gimbal_control.h"
#include "gimbal_tx.h"
#include "gimbal_proto_rs3.h"
#include "ds4_host.h"
#include "matrix_status.h"
#include "debug_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_gatt_common_api.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

enum { APP_ID=0x43, EVENT_ADV=1000, EVENT_SCAN_DONE, EVENT_COMMAND };
enum { CMD_ON, CMD_OFF, CMD_PAIR, CMD_STOP, CMD_SPEED, CMD_SPEED_PAN, CMD_SPEED_TILT, CMD_INVERT, CMD_CALIBRATE };
typedef struct {
    unsigned event, status, value;
    uint16_t connection, handle, end, uuid;
    esp_gatt_if_t iface;
    uint8_t address[6], address_type;
    uint16_t size;
    uint8_t bytes[RS3_FRAME_MAX];
} link_event_t;
typedef struct {
    uint8_t version, enabled, paired, address_type, address[6];
    int8_t offset_x, offset_y;
    uint8_t invert_y;
    uint16_t span;
} config_t;
static const char *TAG="gimbal";
static QueueHandle_t events;
static atomic_uint iface=ESP_GATT_IF_NONE, published_link, published_span=120, published_tilt_span=120;
static atomic_bool overflow, fault, published_enabled;
static atomic_uint rx_frames, rx_at, tx_control, tx_neutral, tx_centers, pending_writes;
static atomic_bool published_armed;
static atomic_uint published_phase, scan_named, scan_saved, published_failures;
static config_t cfg;
/* Separate key keeps the existing v1 blob and peer binding compatible. */
static uint16_t tilt_span=120;
/* GAP runs on the Bluetooth task; publish the saved peer as one snapshot. */
static portMUX_TYPE target_lock=portMUX_INITIALIZER_UNLOCKED;
static bool target_saved;
static uint8_t target_address[6];
static nvs_handle_t storage;
static bool registered, scanning, opening, opened, searching, subscribed, writable, closing;
static bool session_ready;
static uint8_t notify_properties;
static uint32_t session_at;
static uint16_t conn, service_start, service_end, write_handle, notify_handle, cccd, sequence;
static uint8_t peer[6], peer_type;
static uint8_t chosen[6], chosen_type;
static unsigned candidates, failures;
static uint32_t next_scan, started, next_poll, last_rx, battery_at;
static bool received, stopped, have_battery, need_stop;
static rs3_stream_t stream;
static portMUX_TYPE pose_lock=portMUX_INITIALIZER_UNLOCKED;
static rs3_pose_raw_t latest_pose;
static uint32_t pose_at;
static bool pose_valid;
static uint8_t pose_sender;
static gimbal_control_t control;
static gimbal_tx_t tx;
typedef enum { WRITE_ACCEPTED, WRITE_DEFERRED, WRITE_FAILED } write_result_t;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now-at)>=0; }
uint8_t gimbal_link_state(void) { return (uint8_t)atomic_load(&published_link); }
bool gimbal_link_fault(void) { return atomic_load(&fault); }
static void state(matrix_link_t link)
{ atomic_store(&published_link,link); matrix_status_set_ble_gimbal(link); }
static void post(const link_event_t *event)
{ if (events && xQueueSend(events,event,0)!=pdTRUE) atomic_store(&overflow,true); }
static void remember_target(void)
{
    portENTER_CRITICAL(&target_lock);
    target_saved=cfg.paired; memcpy(target_address,cfg.address,6);
    portEXIT_CRITICAL(&target_lock);
}
void gimbal_gap_event(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *p)
{
    if (event==ESP_GAP_BLE_SCAN_START_COMPLETE_EVT && p->scan_start_cmpl.status!=ESP_BT_STATUS_SUCCESS) {
        post(&(link_event_t){.event=EVENT_SCAN_DONE});
    } else if (event==ESP_GAP_BLE_SCAN_RESULT_EVT) {
        if (p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_CMPL_EVT) post(&(link_event_t){.event=EVENT_SCAN_DONE});
        else if (p->scan_rst.search_evt==ESP_GAP_SEARCH_INQ_RES_EVT) {
            ble_advertisement_t adv={0};
            bool valid=ble_advertisement_parse(&adv,p->scan_rst.ble_adv,p->scan_rst.adv_data_len) &&
                ble_advertisement_parse(&adv,p->scan_rst.ble_adv+p->scan_rst.adv_data_len,p->scan_rst.scan_rsp_len);
            if (valid && rs3_name(adv.name)) ++scan_named;
            portENTER_CRITICAL(&target_lock);
            bool candidate=rs3_candidate(target_saved,target_address,p->scan_rst.bda,valid?adv.name:NULL);
            portEXIT_CRITICAL(&target_lock);
            if (candidate) {
                link_event_t e={.event=EVENT_ADV,.address_type=p->scan_rst.ble_addr_type};
                memcpy(e.address,p->scan_rst.bda,6); post(&e);
            }
        }
    }
    /* This protocol client does not request SMP bonding; the BLE HID client
     * handles only its own peer. Firmware requiring additional authentication
     * must be adapted through its normal pairing flow before enabling control. */
}
void gimbal_gatt_event(esp_gattc_cb_event_t event, esp_gatt_if_t client, esp_ble_gattc_cb_param_t *p)
{
    if (event==ESP_GATTC_REG_EVT) {
        if (p->reg.app_id!=APP_ID) return;
        atomic_store(&iface,client);
        post(&(link_event_t){.event=event,.status=p->reg.status,.iface=client}); return;
    }
    if (client!=atomic_load(&iface)) return;
    link_event_t e={.event=event,.iface=client};
    switch (event) {
    case ESP_GATTC_OPEN_EVT:
        e.status=p->open.status; e.connection=p->open.conn_id;
        memcpy(e.address,p->open.remote_bda,6); break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT: e.status=p->dis_srvc_cmpl.status; e.connection=p->dis_srvc_cmpl.conn_id; break;
    case ESP_GATTC_CFG_MTU_EVT: e.status=p->cfg_mtu.status; e.connection=p->cfg_mtu.conn_id; e.value=p->cfg_mtu.mtu; break;
    case ESP_GATTC_SEARCH_RES_EVT:
        e.connection=p->search_res.conn_id; e.handle=p->search_res.start_handle; e.end=p->search_res.end_handle;
        if (p->search_res.srvc_id.uuid.len==ESP_UUID_LEN_16) e.uuid=p->search_res.srvc_id.uuid.uuid.uuid16;
        break;
    case ESP_GATTC_SEARCH_CMPL_EVT: e.status=p->search_cmpl.status; e.connection=p->search_cmpl.conn_id; break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: e.status=p->reg_for_notify.status; e.handle=p->reg_for_notify.handle; break;
    case ESP_GATTC_WRITE_DESCR_EVT: e.status=p->write.status; e.connection=p->write.conn_id; e.handle=p->write.handle; break;
    case ESP_GATTC_WRITE_CHAR_EVT: e.status=p->write.status; e.connection=p->write.conn_id; e.handle=p->write.handle; break;
    case ESP_GATTC_NOTIFY_EVT:
        e.connection=p->notify.conn_id; e.handle=p->notify.handle; e.size=p->notify.value_len;
        if (e.size>sizeof(e.bytes)) { atomic_store(&overflow,true); return; }
        memcpy(e.bytes,p->notify.value,e.size); break;
    case ESP_GATTC_DISCONNECT_EVT: e.connection=p->disconnect.conn_id; e.status=p->disconnect.reason; break;
    case ESP_GATTC_CLOSE_EVT: e.connection=p->close.conn_id; e.status=p->close.status; break;
    default: return;
    }
    post(&e);
}
static bool save(void)
{
    remember_target();
    esp_err_t err=nvs_set_blob(storage,"cfg",&cfg,sizeof(cfg));
    if (err==ESP_OK) err=nvs_set_u16(storage,"tilt_span",tilt_span);
    if (err==ESP_OK) err=nvs_commit(storage);
    if (err!=ESP_OK) ESP_LOGE(TAG,"Config save failed: %s",esp_err_to_name(err));
    published_span=cfg.span; published_tilt_span=tilt_span; published_enabled=cfg.enabled;
    return err==ESP_OK;
}
static write_result_t write_frame(const uint8_t *bytes, size_t n, gimbal_tx_kind_t kind)
{
    if (!opened || closing || !write_handle || !n) return WRITE_FAILED;
    if (!gimbal_tx_available(&tx,kind)) return WRITE_DEFERRED;
    esp_err_t err=esp_ble_gattc_write_char(atomic_load(&iface),conn,write_handle,n,(uint8_t *)bytes,
                                         ESP_GATT_WRITE_TYPE_NO_RSP,ESP_GATT_AUTH_REQ_NONE);
    if (err!=ESP_OK) { ++failures; need_stop=true; control.armed=false; ESP_LOGW(TAG,"Write rejected: %s",esp_err_to_name(err)); return WRITE_FAILED; }
    if (bytes[9]==4 && (bytes[10]==1 || bytes[10]==0x4c)) {
        ESP_LOGD(TAG,"Control TX id=0x%02x kind=%u seq=%u",bytes[10],(unsigned)kind,(unsigned)(bytes[6]|(bytes[7]<<8)));
        ESP_LOG_BUFFER_HEX_LEVEL(TAG,bytes,n,ESP_LOG_DEBUG);
    }
    gimbal_tx_accepted(&tx,kind,now_ms());
    if (kind==GIMBAL_TX_NEUTRAL) ++tx_neutral;
    else if (bytes[9]==4 && bytes[10]==1) ++tx_control;
    else if (bytes[9]==4 && bytes[10]==0x4c) ++tx_centers;
    return WRITE_ACCEPTED;
}
static bool stop(void)
{
    uint8_t frame[22]; size_t n=rs3_stick(frame,sizeof(frame),sequence++,0,0);
    write_result_t result=write_frame(frame,n,GIMBAL_TX_NEUTRAL);
    need_stop=result!=WRITE_ACCEPTED; return result==WRITE_ACCEPTED;
}
static void close_link(const char *reason, bool error)
{
    portENTER_CRITICAL(&pose_lock); pose_valid=false; portEXIT_CRITICAL(&pose_lock);
    have_battery=false; matrix_status_set_ble_gimbal_battery(255);
    if (error) { atomic_store(&fault,true); matrix_status_set_gimbal_fault(true); }
    if (opened && !closing) {
        stop(); closing=true;
        esp_ble_gattc_close(atomic_load(&iface),conn);
        /* close releases the virtual GATT client; retire this peer's physical
         * link as well so it can advertise again. Other peers are unaffected. */
        esp_ble_gap_disconnect(peer);
    }
    else if (opening && !closing) {
        closing=true;
        esp_ble_gap_disconnect(peer);
    }
    writable=false; state(cfg.enabled?MATRIX_SEARCHING:MATRIX_DISCONNECTED);
    started=now_ms(); ESP_LOGW(TAG,"Link closed: %s",reason);
}
static void reset_link(void)
{
    portENTER_CRITICAL(&pose_lock); pose_valid=false; portEXIT_CRITICAL(&pose_lock);
    matrix_status_set_ble_gimbal_battery(255);
    opening=opened=searching=subscribed=writable=closing=false;
    session_ready=false; notify_properties=0;
    service_start=service_end=write_handle=notify_handle=cccd=0;
    received=stopped=have_battery=need_stop=false; failures=0; stream.used=0; control=(gimbal_control_t){0};
    tx=(gimbal_tx_t){0};
    next_scan=now_ms()+3000; state(cfg.enabled?MATRIX_SEARCHING:MATRIX_DISCONNECTED);
}
static void decoded(void *context, const uint8_t *p, size_t n)
{
    (void)context;
    /* Only frames addressed to this app from actual gimbal endpoints count. */
    if (p[5]!=2 || (p[4]!=4 && p[4]!=0xe5 && p[4]!=0x27)) return;
    received=true; last_rx=now_ms();
    ++rx_frames; rx_at=last_rx;
    rs3_pose_raw_t pose;
    if (rs3_pose_raw(p,n,&pose)) {
        portENTER_CRITICAL(&pose_lock);
        latest_pose=pose; pose_at=last_rx; pose_valid=true; pose_sender=p[4];
        portEXIT_CRITICAL(&pose_lock);
    }
    if (p[9]==4 && (p[10]==1 || p[10]==0x4c || p[10]==0x0f)) {
        ESP_LOGI(TAG,"Control RX sender=0x%02x id=0x%02x flags=0x%02x seq=%u payload_bytes=%u",
            p[4],p[10],p[8],(unsigned)(p[6]|(p[7]<<8)),(unsigned)(n-13));
        ESP_LOG_BUFFER_HEX_LEVEL(TAG,p+11,n-13,ESP_LOG_INFO);
    }
    if (p[9]==4 && (p[10]==0x66 || p[10]==0x10)) {
        ESP_LOGD(TAG,"Pose packet id=0x%02x payload_bytes=%u",p[10],(unsigned)(n-13));
        ESP_LOG_BUFFER_HEX_LEVEL(TAG,p+11,n-13,ESP_LOG_DEBUG);
    }
    uint8_t percent;
    if (rs3_battery(p,n,&percent)) {
        matrix_status_set_ble_gimbal_battery(percent); have_battery=true; battery_at=last_rx;
    }
}
static bool characteristic(uint16_t uuid, uint8_t property, uint16_t *handle, uint8_t *properties)
{
    esp_bt_uuid_t id={.len=ESP_UUID_LEN_16,.uuid.uuid16=uuid};
    esp_gattc_char_elem_t item; uint16_t count=1;
    esp_gatt_status_t status=esp_ble_gattc_get_char_by_uuid(atomic_load(&iface),conn,service_start,service_end,id,&item,&count);
    if (status!=ESP_GATT_OK || count!=1) {
        ESP_LOGW(TAG,"Characteristic %04x lookup status=%u count=%u",uuid,status,count);return false;
    }
    ESP_LOGI(TAG,"Characteristic %04x handle=%u properties=0x%02x required=0x%02x",uuid,item.char_handle,item.properties,property);
    if (!(item.properties&property)) return false;
    *handle=item.char_handle; if (properties) *properties=item.properties; return true;
}
static void handle_event(const link_event_t *e)
{
    uint32_t now=now_ms();
    if (e->event==EVENT_COMMAND) {
        switch (e->value) {
        case CMD_OFF: stop(); cfg.enabled=0; close_link("disabled",false); save(); break;
        case CMD_ON:
            if (!cfg.enabled) { cfg.enabled=1; save(); next_scan=now; }
            state(writable && !closing?MATRIX_CONNECTED:MATRIX_SEARCHING); break;
        case CMD_PAIR: stop(); close_link("new pairing",false); cfg.paired=0; memset(cfg.address,0,6); cfg.enabled=1; save(); break;
        case CMD_STOP: stop(); control.armed=false; control.moving=control.centering=false; break;
        case CMD_SPEED: stop(); cfg.span=tilt_span=e->status; control.armed=false; save(); break;
        case CMD_SPEED_PAN: stop(); cfg.span=e->status; control.armed=false; save(); break;
        case CMD_SPEED_TILT: stop(); tilt_span=e->status; control.armed=false; save(); break;
        case CMD_INVERT: stop(); cfg.invert_y=e->status; control.armed=false; save(); break;
        case CMD_CALIBRATE: {
            ds4_state_t pad; uint32_t report,epoch; ds4_host_get_gimbal(&pad,&report,&epoch);
            if (!pad.connected || (uint32_t)(now-report)>=200 || abs(pad.lx)>32 || abs(pad.ly)>32)
                ESP_LOGW(TAG,"Calibration requires fresh, centered DS4 input");
            else { stop(); cfg.offset_x=pad.lx; cfg.offset_y=pad.ly; control.armed=false; save(); }
            break;
        }
        }
        return;
    }
    if (e->event==ESP_GATTC_REG_EVT) { registered=e->status==ESP_GATT_OK; return; }
    if (e->event==EVENT_ADV && scanning && cfg.enabled) {
        if (cfg.paired && memcmp(e->address,cfg.address,6)) return;
        ++scan_saved;
        if (!candidates) { memcpy(chosen,e->address,6); chosen_type=e->address_type; candidates=1; }
        else if (memcmp(chosen,e->address,6)) candidates=2;
        return;
    }
    if (e->event==EVENT_SCAN_DONE) {
        scanning=false; next_scan=now+3000;
        if (candidates==1 && cfg.enabled && !opened && !opening) {
            memcpy(peer,chosen,6); peer_type=chosen_type; opening=true; started=now; state(MATRIX_CONNECTING);
            ESP_LOGI(TAG,"One RS3 Mini found; connecting");
            if (esp_ble_gattc_open(atomic_load(&iface),peer,peer_type,true)!=ESP_OK) reset_link();
        } else if (candidates>1) ESP_LOGW(TAG,"Multiple RS3 Minis; pair with only intended unit powered on");
        return;
    }
    if (e->event==ESP_GATTC_OPEN_EVT) {
        if (e->status!=ESP_GATT_OK) { reset_link(); return; }
        conn=e->connection; opened=true; opening=false;
        if (!cfg.enabled || closing || memcmp(peer,e->address,6)) { closing=false; close_link("retired open",false); return; }
        started=now; sequence=0x1700; stream.used=0; received=stopped=false;
        if (esp_ble_gattc_send_mtu_req(atomic_load(&iface),conn)!=ESP_OK) close_link("MTU request failed",true);
        return;
    }
    if (!opened || (e->event!=ESP_GATTC_REG_FOR_NOTIFY_EVT && e->connection!=conn)) return;
    if (e->event==ESP_GATTC_DISCONNECT_EVT) { ESP_LOGI(TAG,"Disconnected reason=%u",e->status); reset_link(); return; }
    if (e->event==ESP_GATTC_CLOSE_EVT) {
        if (e->status==ESP_GATT_OK) { ESP_LOGI(TAG,"GATT client closed"); reset_link(); }
        else ESP_LOGW(TAG,"GATT close rejected status=%u",e->status);
        return;
    }
    if (closing) return;
    switch (e->event) {
    case ESP_GATTC_CFG_MTU_EVT:
        /* Every control frame is 22 bytes: require ATT MTU >= 25, no silent truncation. */
        if (e->status!=ESP_GATT_OK || e->value<25) { close_link("MTU too small",true); break; }
        searching=true;
        if (esp_ble_gattc_search_service(atomic_load(&iface),conn,NULL)!=ESP_OK) close_link("service search failed",true);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
        if (e->uuid==0xfff0) { service_start=e->handle; service_end=e->end; } break;
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(TAG,"Service search status=%u active=%d FFF0=%u..%u",e->status,searching,service_start,service_end);
        if (!searching || e->status!=ESP_GATT_OK || !service_start ||
            !characteristic(0xfff5,ESP_GATT_CHAR_PROP_BIT_WRITE_NR,&write_handle,NULL) ||
            !characteristic(0xfff4,ESP_GATT_CHAR_PROP_BIT_NOTIFY,&notify_handle,&notify_properties)) { close_link("control service unavailable",true); break; }
        if (esp_ble_gattc_register_for_notify(atomic_load(&iface),peer,notify_handle)!=ESP_OK) close_link("notify registration failed",true);
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        if (e->status!=ESP_GATT_OK || e->handle!=notify_handle) { close_link("notify registration rejected",true); break; }
        esp_bt_uuid_t id={.len=ESP_UUID_LEN_16,.uuid.uuid16=0x2902};
        esp_gattc_descr_elem_t item; uint16_t count=1; uint8_t on[]={1,0};
        if (esp_ble_gattc_get_descr_by_char_handle(atomic_load(&iface),conn,notify_handle,id,&item,&count)!=ESP_GATT_OK || count!=1) {
            close_link("CCCD unavailable",true); break;
        }
        cccd=item.handle;
        if (esp_ble_gattc_write_char_descr(atomic_load(&iface),conn,cccd,2,on,ESP_GATT_WRITE_TYPE_RSP,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK)
            close_link("CCCD write failed",true);
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        if (e->handle==cccd) {
            if (e->status!=ESP_GATT_OK) close_link("CCCD rejected",true);
            else {
                static const uint8_t pairing[]={1,0};
                subscribed=true;
                if (notify_properties&(ESP_GATT_CHAR_PROP_BIT_WRITE|ESP_GATT_CHAR_PROP_BIT_WRITE_NR)) {
                    /* Some vendor revisions expose an additional characteristic
                     * session write. A notify-only FFF4 uses its CCCD instead. */
                    esp_gatt_write_type_t type=(notify_properties&ESP_GATT_CHAR_PROP_BIT_WRITE)?ESP_GATT_WRITE_TYPE_RSP:ESP_GATT_WRITE_TYPE_NO_RSP;
                    if (esp_ble_gattc_write_char(atomic_load(&iface),conn,notify_handle,sizeof(pairing),(uint8_t *)pairing,type,ESP_GATT_AUTH_REQ_NONE)!=ESP_OK)
                        close_link("FFF4 session write failed",true);
                } else {
                    session_ready=true;session_at=now+300;next_poll=session_at;
                    ESP_LOGI(TAG,"Notify-only FFF4 session initialized through CCCD");
                }
            }
        }
        break;
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (e->handle==notify_handle && !session_ready) {
            if (e->status!=ESP_GATT_OK) close_link("FFF4 session rejected",true);
            else { session_ready=true; session_at=now+300; next_poll=session_at; ESP_LOGI(TAG,"FFF4 session initialized"); }
        } else if (e->handle==write_handle) {
            gimbal_tx_kind_t kind;
            if (!gimbal_tx_complete(&tx,&kind)) break;
            if (e->status!=ESP_GATT_OK) { ++failures; need_stop=true; control.armed=false; }
            else { failures=0; if (kind==GIMBAL_TX_NEUTRAL) need_stop=false; }
        }
        break;
    case ESP_GATTC_NOTIFY_EVT:
        if (subscribed && e->handle==notify_handle) rs3_stream_feed(&stream,e->bytes,e->size,decoded,NULL);
        break;
    default: break;
    }
}
static void worker(void *arg)
{
    (void)arg;
    for (;;) {
        link_event_t event;
        for (unsigned i=0;i<8 && xQueueReceive(events,&event,0)==pdTRUE;++i) handle_event(&event);
        uint32_t now=now_ms();
        if (atomic_exchange(&overflow,false)) { close_link("event queue overflow",true); }
        if (closing && (uint32_t)(now-started)>3000) {
            /* Retry close/cancel until the stack reports terminal state; don't
             * forget an outstanding open and create a second connection. */
            if (opened) esp_ble_gattc_close(atomic_load(&iface),conn);
            if (opened || opening) esp_ble_gap_disconnect(peer);
            else reset_link();
            started=now;
        } else if (!closing && (opening || (opened && !writable)) && (uint32_t)(now-started)>15000) close_link("setup timeout",true);
        if (cfg.enabled && registered && !opened && !opening && !closing && !scanning && due(now,next_scan)) {
            candidates=0; scanning=true;
            esp_err_t err=ble_clients_scan(2,3);
            if (err!=ESP_OK) scanning=false;
            next_scan=now+(err==ESP_OK?6000:1000); state(MATRIX_SEARCHING);
        }
        if (opened && subscribed && session_ready && due(now,session_at) && !closing) {
            if (gimbal_tx_expired(&tx,now)) { close_link("write completion timeout",true); }
            if (closing) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }
            if (!stopped || need_stop) stopped=stop();
            if (stopped && received && !writable && cfg.enabled) {
                writable=true; state(MATRIX_CONNECTED); atomic_store(&fault,false); matrix_status_set_gimbal_fault(false);
                if (!cfg.paired) { cfg.paired=1; memcpy(cfg.address,peer,6); cfg.address_type=peer_type; save(); }
                ESP_LOGI(TAG,"RS3 Mini control ready; center DS4 stick before moving");
            }
            if (due(now,next_poll)) {
                static const uint8_t payload[]={0x10,0x51,1,0,0,0,0x0c,0,0,0x50,0,0xf1,3,0x66,0x24,0xc0,0x1d,0,0,0x1c};
                /* The Mini reference's steady-state builder addresses endpoint
                 * 4. This setting restored actual control in our hardware test;
                 * captured E5 polls alone had only established telemetry. */
                uint8_t frame[40]; size_t n=rs3_frame(frame,sizeof(frame),sequence++,4,0,4,0x12,payload,sizeof(payload));
                write_result_t result=write_frame(frame,n,GIMBAL_TX_CONTROL);
                if (result!=WRITE_DEFERRED) next_poll=now+1000;
            }
            if (writable && (uint32_t)(now-last_rx)>5000) close_link("control response timeout",true);
            if (have_battery && (uint32_t)(now-battery_at)>15000) { have_battery=false; matrix_status_set_ble_gimbal_battery(255); }
            if (writable && !closing && !need_stop) {
                ds4_state_t pad; uint32_t report,epoch; ds4_host_get_gimbal(&pad,&report,&epoch);
                gimbal_input_t in={.connected=pad.connected,.x=pad.lx,.y=pad.ly,.l3=(pad.buttons&(1u<<1))!=0,.report_ms=report,.epoch=epoch};
                gimbal_tuning_t tuning={.offset_x=cfg.offset_x,.offset_y=cfg.offset_y,.invert_y=cfg.invert_y,.span=cfg.span,.tilt_span=tilt_span};
                gimbal_control_t before=control;
                gimbal_output_t out=gimbal_control_step(&control,&tuning,&in,true,now);
                uint8_t frame[22]; size_t n=0;
                if (out.action==GIMBAL_CENTER && before.moving) {
                    /* Submit neutral once, retain the L3 edge until its write
                     * completes, then submit center in a later task iteration. */
                    unsigned prior_failures=failures;
                    if (stop()) before.moving=false;
                    control=before;
                    if (failures!=prior_failures) control.armed=false;
                }
                else if (out.action==GIMBAL_CENTER) n=rs3_center(frame,sizeof(frame),sequence++);
                else if (out.action!=GIMBAL_NONE) n=rs3_stick(frame,sizeof(frame),sequence++,out.pan,out.tilt);
                if (n) {
                    gimbal_tx_kind_t kind=out.action==GIMBAL_STOP?GIMBAL_TX_NEUTRAL:GIMBAL_TX_CONTROL;
                    write_result_t result=write_frame(frame,n,kind);
                    if (result!=WRITE_ACCEPTED) control=before;
                    if (result==WRITE_FAILED) { control.armed=false; need_stop=true; }
                }
            }
            if (failures>=5) close_link("five write failures",true);
        }
        pending_writes=tx.count; published_armed=writable && !closing && control.armed;
        published_phase=(registered?1:0)|(scanning?2:0)|(opening?4:0)|(opened?8:0)|
            (subscribed?16:0)|(session_ready?32:0)|(closing?64:0)|(writable?128:0);
        published_failures=failures;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
bool gimbal_link_command(int argc, char **argv)
{
    if (strcmp(argv[0],"gimbal")) return false;
    if (argc==2 && !strcmp(argv[1],"status")) {
        debug_printf("[dbg] OK gimbal state=%u enabled=%d fault=%d span=%u tilt_span=%u protocol=rs3-mini-unofficial\n",
            gimbal_link_state(),atomic_load(&published_enabled),gimbal_link_fault(),atomic_load(&published_span),atomic_load(&published_tilt_span));
        ds4_state_t pad; uint32_t report,epoch; ds4_host_get_gimbal(&pad,&report,&epoch);
        uint32_t now=now_ms();
        debug_printf("[dbg] gimbal armed=%d ds4=%d input_age_ms=%lu rx_frames=%u rx_age_ms=%lu tx_pending=%u move_queued=%u neutral_queued=%u center_queued=%u\n",
            atomic_load(&published_armed),pad.connected,(unsigned long)(pad.connected?now-report:UINT32_MAX),
            atomic_load(&rx_frames),(unsigned long)(atomic_load(&rx_frames)?now-atomic_load(&rx_at):UINT32_MAX),
            atomic_load(&pending_writes),atomic_load(&tx_control),atomic_load(&tx_neutral),atomic_load(&tx_centers));
        debug_printf("[dbg] gimbal phase=0x%02x scan_named=%u scan_saved=%u write_failures=%u\n",
            atomic_load(&published_phase),atomic_load(&scan_named),atomic_load(&scan_saved),atomic_load(&published_failures));
        portENTER_CRITICAL(&pose_lock);
        rs3_pose_raw_t pose=latest_pose; bool valid=pose_valid; uint32_t at=pose_at; uint8_t sender=pose_sender;
        portEXIT_CRITICAL(&pose_lock);
        debug_printf("[dbg] gimbal pose_raw_valid=%d pose_age_ms=%lu pan_raw=%d tilt_raw=%d roll_raw=%d source=0x%02x\n",
            valid,(unsigned long)(valid?now-at:UINT32_MAX),valid?pose.pan:0,valid?pose.tilt:0,valid?pose.roll:0,valid?sender:0);
        return true;
    }
    link_event_t e={.event=EVENT_COMMAND}; bool valid=argc==2;
    if (argc>=2 && !strcmp(argv[1],"on")) e.value=CMD_ON;
    else if (argc>=2 && !strcmp(argv[1],"off")) e.value=CMD_OFF;
    else if (argc>=2 && !strcmp(argv[1],"pair")) e.value=CMD_PAIR;
    else if (argc>=2 && !strcmp(argv[1],"stop")) e.value=CMD_STOP;
    else if (argc>=2 && !strcmp(argv[1],"calibrate")) e.value=CMD_CALIBRATE;
    else if (argc==3 && (!strcmp(argv[1],"speed") || !strcmp(argv[1],"invert"))) {
        char *end; long value=strtol(argv[2],&end,10);
        e.value=!strcmp(argv[1],"speed")?CMD_SPEED:CMD_INVERT;
        valid=*argv[2] && !*end && value>=0 && value<=(e.value==CMD_SPEED?400:1) && (e.value!=CMD_SPEED || value>=20);
        e.status=(unsigned)value;
    } else if (argc==4 && !strcmp(argv[1],"speed") && (!strcmp(argv[2],"pan") || !strcmp(argv[2],"tilt"))) {
        char *end; long value=strtol(argv[3],&end,10);
        e.value=!strcmp(argv[2],"pan")?CMD_SPEED_PAN:CMD_SPEED_TILT;
        valid=*argv[3] && !*end && value>=20 && value<=400;
        e.status=(unsigned)value;
    } else valid=false;
    if (!valid) debug_printf("[dbg] ERR gimbal: status|on|off|pair|stop|calibrate|speed [pan|tilt] 20..400|invert 0|1\n");
    else if (!events || xQueueSend(events,&e,0)!=pdTRUE) debug_printf("[dbg] ERR gimbal command queue busy\n");
    else debug_printf("[dbg] OK gimbal command queued\n");
    return true;
}
esp_err_t gimbal_link_init(void)
{
    cfg=(config_t){.version=1,.enabled=1,.invert_y=1,.span=120};
    esp_err_t err=nvs_open("gimbal",NVS_READWRITE,&storage); if (err!=ESP_OK) return err;
    config_t saved; size_t size=sizeof(saved);
    if (nvs_get_blob(storage,"cfg",&saved,&size)==ESP_OK && size==sizeof(saved) && saved.version==1 &&
        saved.enabled<=1 && saved.paired<=1 && saved.invert_y<=1 && saved.span>=20 && saved.span<=400 &&
        abs(saved.offset_x)<=32 && abs(saved.offset_y)<=32 && saved.address_type<=3) cfg=saved;
    tilt_span=cfg.span;
    uint16_t saved_tilt;
    if (nvs_get_u16(storage,"tilt_span",&saved_tilt)==ESP_OK && saved_tilt>=20 && saved_tilt<=400) tilt_span=saved_tilt;
    remember_target();
    published_span=cfg.span; published_tilt_span=tilt_span; published_enabled=cfg.enabled;
    events=xQueueCreate(16,sizeof(link_event_t)); if (!events) return ESP_ERR_NO_MEM;
    state(cfg.enabled?MATRIX_SEARCHING:MATRIX_DISCONNECTED);
    err=esp_ble_gatt_set_local_mtu(185); if (err!=ESP_OK) return err;
    err=esp_ble_gattc_app_register(APP_ID); if (err!=ESP_OK) return err;
    return xTaskCreate(worker,"gimbal",4096,NULL,4,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
