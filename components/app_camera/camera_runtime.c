#include "app_camera.h"
#include "camera_runtime.h"
#include "camera_endpoint.h"
#include "camera_session.h"
#include "camera_discovery.h"
#include "camera_stream.h"
#include "camera_identity_work.h"
#include "camera_link.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#define OBJECT_CAPACITY (512u*1024u)
/* Boot-owned packet storage; leases drain before a new worker can reuse it. */
static uint8_t *packet_buffers[2];
static const char *TAG="camera_pair";
static atomic_bool busy,worker_active,stop_requested,releasing_controls,ready;
static atomic_uint lifetime,frame_generation,stop_ms,properties_ms;
static atomic_uint network_generation;
static uint32_t attempt_network_generation;
static atomic_bool initialized,messages_started;
static atomic_int last_io;
static bool maintenance_gate;
static atomic_bool exclusive_closed;
static bool exclusive_drained;
#if CONFIG_REMOTE_DBG_SIM
static bool display_gate;
#endif
static portMUX_TYPE lifecycle_mux=portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE controls_mux=portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE debug_mux=portMUX_INITIALIZER_UNLOCKED;
static char debug_phase[96]="idle";
static camera_stream_t stream;
static int pending_steps[CAMERA_MENU_COUNT+1];
static uint32_t pending_generation;
static QueueHandle_t focus_requests;
typedef struct { int direction; uint32_t generation,epoch,queued_ms; } focus_request_t;
static uint32_t now_ms(void *unused) { (void)unused; return (uint32_t)(esp_timer_get_time()/1000); }
static void enter_controls(void *unused) { (void)unused; portENTER_CRITICAL(&controls_mux); }
static void leave_controls(void *unused) { (void)unused; portEXIT_CRITICAL(&controls_mux); }
bool camera_controller_active(void) { return atomic_load(&worker_active); }
bool camera_controller_admitting(void) { return !atomic_load(&exclusive_closed); }
uint32_t camera_controller_lifetime(void) { return atomic_load(&lifetime); }
uint32_t camera_controller_frame_generation(void) { return atomic_load(&frame_generation); }
static bool cancelled(void *unused)
{
    (void)unused;
    uint32_t current=atomic_load(&network_generation);
    return (current && current!=attempt_network_generation) || (atomic_load(&stop_requested) &&
        (!atomic_load(&releasing_controls) || (uint32_t)(now_ms(NULL)-atomic_load(&stop_ms))>=900));
}
void camera_controller_network_changed(uint32_t generation)
{
    uint32_t previous=atomic_load(&network_generation);
    if (generation && (!previous || (int32_t)(generation-previous)>0)) {
        atomic_store(&network_generation,generation); camera_focus_cancel();
    }
}
static bool wait_cancel(unsigned ms)
{
    while (ms && !atomic_load(&stop_requested)) { unsigned slice=ms>100?100:ms; vTaskDelay(pdMS_TO_TICKS(slice)); ms-=slice; }
    return !atomic_load(&stop_requested);
}
void camera_focus_cancel(void) { camera_stream_focus_cancel(&stream); if (focus_requests) xQueueReset(focus_requests); }
void camera_gamepad_caps(gamepad_caps_t *out)
{
    if (!out) return;
    enter_controls(NULL); *out=stream.controls.caps; leave_controls(NULL);
    if ((uint32_t)(now_ms(NULL)-atomic_load(&properties_ms))>=6000) out->mf_known=out->zoom_known=out->recording_known=false;
}
bool camera_focus_ready(void)
{
    gamepad_caps_t c; camera_gamepad_caps(&c);
    return camera_controller_admitting() && atomic_load(&ready) && !atomic_load(&stop_requested) && c.session && c.mf_known && c.mf && c.lens==PAD_LENS_NON_POWER_ZOOM;
}
static void enqueue_setting(unsigned property,int direction,uint32_t generation)
{
    if ((direction!=1 && direction!=-1) || !camera_controller_admitting() || !atomic_load(&ready) || atomic_load(&stop_requested)) return;
    unsigned index=CAMERA_MENU_COUNT+1;
    if (property==APP_CAMERA_PROPERTY_MODE) index=0;
    for (unsigned i=0;i<CAMERA_MENU_COUNT;++i) if (camera_menu_properties[i]==property) index=i+1;
    if (index>CAMERA_MENU_COUNT) return;
    enter_controls(NULL); uint32_t current=stream.controls.caps.generation;
    if (stream.controls.caps.session && (!generation || generation==current)) {
        if (pending_generation!=current) { memset(pending_steps,0,sizeof pending_steps); pending_generation=current; }
        if (pending_steps[index]>-1000000 && pending_steps[index]<1000000) pending_steps[index]+=direction;
    }
    leave_controls(NULL);
}
void camera_controller_setting(unsigned property,int direction)
{
    if (property==APP_CAMERA_PROPERTY_MODE || property==APP_CAMERA_PROPERTY_FOCUS) camera_focus_cancel();
    enqueue_setting(property,direction,0);
}
void camera_mode_step(int direction) { camera_controller_setting(APP_CAMERA_PROPERTY_MODE,direction); }
void camera_focus_mode_step(int direction) { camera_controller_setting(APP_CAMERA_PROPERTY_FOCUS,direction); }
static void focus_step_for_generation(int direction,uint32_t generation)
{
    if (!focus_requests || !camera_focus_ready() || (direction!=1 && direction!=-1)) return;
    focus_request_t r={direction,generation,atomic_load(&stream.focus_epoch),now_ms(NULL)}; xQueueOverwrite(focus_requests,&r);
}
void camera_focus_step(int direction) { gamepad_caps_t c; camera_gamepad_caps(&c); focus_step_for_generation(direction,c.generation); }
bool camera_gamepad_action(pad_action_t action)
{
    if (action.type==PAD_ACTION_MF_CANCEL) { camera_focus_cancel(); return true; }
    bool safety=action.type==PAD_ACTION_RELEASE_ALL ||
        ((action.type==PAD_ACTION_S1 || action.type==PAD_ACTION_S2 || action.type==PAD_ACTION_ZOOM) && !action.value);
    if (!camera_controller_admitting() && !safety) return false;
    if (!atomic_load(&ready) || atomic_load(&stop_requested)) return false;
    gamepad_caps_t c; camera_gamepad_caps(&c);
    if (!c.session || action.generation!=c.generation) return false;
    if (action.type==PAD_ACTION_MODE_NEXT || action.type==PAD_ACTION_FOCUS_MODE_NEXT) {
        camera_focus_cancel(); enqueue_setting(action.type==PAD_ACTION_MODE_NEXT?APP_CAMERA_PROPERTY_MODE:APP_CAMERA_PROPERTY_FOCUS,1,action.generation); return true;
    }
    if (action.type==PAD_ACTION_MENU_STEP) {
        return false; /* UI/Input must capture the semantic property in SETTING_ADJUST. */
    }
    if (action.type==PAD_ACTION_MF_STEP) { focus_step_for_generation(action.value,action.generation); return camera_focus_ready(); }
    if (action.type==PAD_ACTION_RECORD_UNAVAILABLE) return true;
    bool accepted=camera_controls_submit(&stream.controls,action,now_ms(NULL));
    if (action.type==PAD_ACTION_RELEASE_ALL || !accepted) camera_focus_cancel();
    return accepted;
}
/* Sole owner merges intake counters. Kernels/frame state never cross tasks. */
static void receive_inputs(void)
{
    int steps[CAMERA_MENU_COUNT+1]; uint32_t generation;
    enter_controls(NULL); generation=stream.controls.caps.generation;
    if (pending_generation==generation) memcpy(steps,pending_steps,sizeof steps); else memset(steps,0,sizeof steps);
    memset(pending_steps,0,sizeof pending_steps); leave_controls(NULL);
    if (stream.focus_pending && stream.focus_request_epoch!=atomic_load(&stream.focus_epoch)) stream.focus_pending=false;
    for (unsigned i=0;i<=CAMERA_MENU_COUNT;++i) if (steps[i]) {
        app_message_t m={.type=APP_MESSAGE_CAMERA_SETTING_ADJUST}; int direction=steps[i]>0?1:-1;
        m.payload.command.index=i?camera_menu_properties[i-1]:APP_CAMERA_PROPERTY_MODE;
        m.payload.command.direction=direction; m.payload.command.token=generation;
        if (camera_stream_message(&stream,&m,now_ms(NULL))==ESP_OK) {
            int *merged=i?&stream.menu_steps[i-1]:&stream.mode_steps;
            int64_t total=(int64_t)*merged+steps[i]-direction;
            *merged=total>1000000?1000000:total< -1000000?-1000000:(int)total;
        }
    }
    focus_request_t r;
    if (xQueueReceive(focus_requests,&r,0)==pdTRUE && r.epoch==atomic_load(&stream.focus_epoch) && (uint32_t)(now_ms(NULL)-r.queued_ms)<500) {
        app_message_t m={.type=APP_MESSAGE_CAMERA_ACTION}; m.payload.action=(pad_action_t){PAD_ACTION_MF_STEP,r.direction,r.generation};
        if (camera_stream_message(&stream,&m,now_ms(NULL))==ESP_OK) { stream.focus_at=r.queued_ms; stream.focus_request_epoch=r.epoch; }
    }
}
static void receive_frame_results(void)
{
    app_message_t m;
    while (camera_endpoint_frame_result(&m)) {
        if (m.result!=ESP_OK && m.result!=ESP_ERR_NOT_FINISHED)
            ESP_LOGW(TAG,"UI frame result: token=%lu error=%s",(unsigned long)m.payload.command.token,esp_err_to_name(m.result));
        camera_stream_message(&stream,&m,now_ms(NULL)); app_message_release(&m);
    }
}
static void connection_status(uint32_t generation,app_camera_stage_t stage,const char *phase,const camera_peer_t *peer)
{
    portENTER_CRITICAL(&debug_mux); snprintf(debug_phase,sizeof debug_phase,"%s",phase); portEXIT_CRITICAL(&debug_mux);
    app_camera_status_t s={.busy=atomic_load(&worker_active),.stopped=atomic_load(&stop_requested),.session=stage==APP_CAMERA_STAGE_LIVE,.last_io=atomic_load(&last_io),.stage=stage};
    snprintf(s.phase,sizeof s.phase,"%s",phase);
    if (peer) { memcpy(s.model,peer->model,sizeof s.model); memcpy(s.firmware,peer->firmware,sizeof s.firmware); }
    camera_outputs_state(generation,&s);
}
static void close_owned(camera_session_t *s)
{
    while (s->backend) {
        camera_backend_result_t result=camera_session_close(s,1000);
        if (result!=CAMERA_BACKEND_OK) { ESP_LOGW(TAG,"Cleanup retains backend: result=%d",result); vTaskDelay(pdMS_TO_TICKS(100)); }
    }
}
/* Core session admission; exclusive mode can deny a new normal session. */
static bool session_notify(uint32_t generation,bool open)
{
    app_message_t message={.type=APP_MESSAGE_SYSTEM_CAMERA_SESSION,.source=APP_ENDPOINT_CAMERA,
        .target=APP_ENDPOINT_SYSTEM,.flags=APP_MESSAGE_REQUEST,.generation=generation,
        .deadline_us=esp_timer_get_time()+2500000},reply={0};
    message.payload.command.flag=open;
    if (!open) { message.flags=0; return app_console_send(&message)==ESP_OK; }
    esp_err_t error=app_console_request(&message,&reply);
    if (error==ESP_OK) error=reply.result;
    app_message_release(&reply); return error==ESP_OK;
}
static camera_backend_result_t liveview(camera_session_t *session,uint8_t *first,uint8_t *second)
{
    camera_backend_t *backend=camera_session_backend(session);
    if (!backend || !camera_stream_begin(&stream,session->generation,first,second,OBJECT_CAPACITY,PAD_LENS_POWER_ZOOM,now_ms(NULL))) return CAMERA_BACKEND_STATE;
    atomic_store(&frame_generation,session->generation); atomic_store(&ready,true);
    camera_backend_result_t result=CAMERA_BACKEND_OK;
    ESP_LOGI(TAG,"LIVEVIEW RUNNING: RX core=%d, decode core=1, 2x512KiB boot buffers; s stops, j starts",xPortGetCoreID());
    while (!atomic_load(&stop_requested) && !stream.frames.failed) {
        receive_frame_results(); receive_inputs();
        result=camera_stream_tick(&stream,backend,5000,now_ms,NULL);
        uint32_t network=atomic_load(&network_generation);
        if (network && network!=attempt_network_generation) {
            backend->ops->network_changed(backend->context,network); result=CAMERA_BACKEND_CANCELLED;
        }
        atomic_store(&properties_ms,stream.properties_at); atomic_store(&last_io,result);
        if (result!=CAMERA_BACKEND_OK) break;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    ESP_LOGW(TAG,"Live-view ended: backend=%d frame_failed=%d stop=%d shown=%u dropped=%u bad=%u",
        result,stream.frames.failed,atomic_load(&stop_requested),stream.frames.shown,stream.frames.dropped,stream.frames.bad_streak);
    atomic_store(&ready,false); camera_focus_cancel(); camera_stream_stop(&stream,now_ms(NULL));
    if (result==CAMERA_BACKEND_OK || result==CAMERA_BACKEND_NOT_READY || result==CAMERA_BACKEND_REFUSED) {
        uint32_t elapsed=atomic_load(&stop_requested)?now_ms(NULL)-atomic_load(&stop_ms):0;
        if (elapsed<900) {
            atomic_store(&releasing_controls,true); camera_stream_release(&stream,backend,900-elapsed,now_ms,NULL); atomic_store(&releasing_controls,false);
        }
    }
    while (!camera_frames_drained(&stream.frames)) { receive_frame_results(); vTaskDelay(pdMS_TO_TICKS(10)); }
    receive_frame_results(); atomic_store(&frame_generation,0);
    if (stream.frames.failed && result==CAMERA_BACKEND_OK) result=CAMERA_BACKEND_STATE;
    close_owned(session); camera_stream_end(&stream);
    ESP_LOGI(TAG,"UI frame leases drained: shown=%u dropped=%u bad=%u",stream.frames.shown,stream.frames.dropped,stream.frames.bad_streak);
    return result;
}
static void pair_task(void *argument)
{
    bool jpeg=(uintptr_t)argument!=0,verify=false;
    camera_identity_t identity={0}; camera_session_t session={0}; uint8_t **buffers=packet_buffers; unsigned failures=0;
    uint32_t generation=camera_session_next_generation();
    camera_backend_result_t result=camera_session_init(&session,&camera_backend_default_factory,cancelled,NULL);
    if (result!=CAMERA_BACKEND_OK || !camera_identity_run(&identity,CAMERA_IDENTITY_LOAD,NULL,NULL)) {
        connection_status(generation,APP_CAMERA_STAGE_FAILED,"Pairing data invalid - stop then u",NULL); goto finished;
    }
    while (!atomic_load(&stop_requested)) {
        attempt_network_generation=atomic_load(&network_generation);
        generation=camera_session_next_generation();
        connection_status(generation,APP_CAMERA_STAGE_DISCOVERING,identity.paired?"Waiting for paired camera...":"Wi-Fi ready - discovering camera...",NULL);
        camera_discovery_t discovery={.selected=-1};
        result=camera_discovery_scan(&session,generation,identity.paired,identity.peer,&discovery); close_owned(&session); atomic_store(&last_io,result);
        if (atomic_load(&stop_requested)) break;
        if (result!=CAMERA_BACKEND_OK || discovery.selected<0) {
            if (verify) { connection_status(generation,APP_CAMERA_STAGE_FAILED,"Reconnect failed - press p to retry",NULL); break; }
            if (discovery.selected==-2) connection_status(generation,APP_CAMERA_STAGE_DISCOVERING,"Multiple cameras - disconnect others",NULL);
            bool failed=discovery.connect_failed || result!=CAMERA_BACKEND_OK;
            unsigned delay=failed?camera_retry_delay(failures):1; if (failed && failures<5) ++failures;
            if (!wait_cancel(delay*1000)) break;
            continue;
        }
        camera_connection_t c={.port=15740,.require_peer_guid=identity.paired,.handshake_timeout_ms=identity.paired?10000:120000};
        memcpy(c.address,discovery.peer.ip,sizeof c.address); memcpy(c.local_guid,identity.guid,sizeof c.local_guid); memcpy(c.peer_guid,identity.peer+6,sizeof c.peer_guid);
        snprintf(c.local_name,sizeof c.local_name,"ESP32-Camera-Remote");
        connection_status(generation,APP_CAMERA_STAGE_CONNECTING,identity.paired?"Reconnecting to paired camera...":"Pairing - confirm on camera...",NULL);
        camera_peer_t peer={0};
        result=camera_session_open(&session,&c,buffers[0],OBJECT_CAPACITY,identity.paired?50000:160000,&peer);
        generation=session.generation; atomic_store(&last_io,result);
        if (result==CAMERA_BACKEND_OK && cancelled(NULL)) result=CAMERA_BACKEND_CANCELLED;
        if (result==CAMERA_BACKEND_OK && !camera_identity_run(&identity,CAMERA_IDENTITY_CONFIRM,discovery.peer.mac,peer.guid)) result=CAMERA_BACKEND_IDENTITY;
        if (result==CAMERA_BACKEND_OK) {
            ESP_LOGI(TAG,"SESSION VERIFIED: complete backend initialization accepted");
            if (!session_notify(generation,true) || cancelled(NULL)) result=CAMERA_BACKEND_CANCELLED;
            else if (jpeg) {
                connection_status(generation,APP_CAMERA_STAGE_LIVE,"Connected - waiting for preview...",&peer);
                result=liveview(&session,buffers[0],buffers[1]); if (stream.frames.shown) failures=0;
            }
        }
        close_owned(&session); session_notify(generation,false); atomic_store(&last_io,result);
        if (atomic_load(&stop_requested)) break;
        if (result==CAMERA_BACKEND_IDENTITY) { connection_status(generation,APP_CAMERA_STAGE_FAILED,"Pairing rejected - confirm camera then j/p",&peer); break; }
        if (!jpeg) {
            if (result==CAMERA_BACKEND_OK && !verify) { verify=true; if (wait_cancel(300)) continue; }
            connection_status(generation,result==CAMERA_BACKEND_OK?APP_CAMERA_STAGE_CONNECTING:APP_CAMERA_STAGE_FAILED,
                result==CAMERA_BACKEND_OK?"Pairing verified - press j for preview":"Pairing failed - press p to retry",&peer); break;
        }
        unsigned delay=camera_retry_delay(failures); if (failures<5) ++failures;
        char message[80]; snprintf(message,sizeof message,"Disconnected - retrying in %u seconds...",delay);
        connection_status(generation,APP_CAMERA_STAGE_FAILED,message,&peer); if (!wait_cancel(delay*1000)) break;
    }
finished:
    atomic_store(&ready,false); camera_focus_cancel(); close_owned(&session); session_notify(generation,false);
    camera_controls_session(&stream.controls,false,PAD_LENS_POWER_ZOOM);
    enter_controls(NULL); memset(pending_steps,0,sizeof pending_steps); leave_controls(NULL);
    camera_network_select(generation,NULL);
    if (atomic_load(&stop_requested)) { connection_status(generation,APP_CAMERA_STAGE_STOPPED,"Stopped - last frame retained",NULL); ESP_LOGI(TAG,"LIVEVIEW STOPPED: channels closed; last frame retained"); }
    ESP_LOGI(TAG,"Camera task finished");
    portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy,false); atomic_store(&worker_active,false); portEXIT_CRITICAL(&lifecycle_mux); vTaskDeleteWithCaps(NULL);
}
static bool start_request(bool jpeg)
{
    if (!atomic_load(&initialized) || !atomic_load(&messages_started)) return false;
    portENTER_CRITICAL(&lifecycle_mux); bool blocked=maintenance_gate || !camera_controller_admitting() || atomic_load(&busy);
    if (!blocked) { atomic_store(&busy,true); atomic_store(&worker_active,true); atomic_store(&stop_requested,false); if (atomic_fetch_add(&lifetime,1)==UINT32_MAX) atomic_store(&lifetime,1); }
    portEXIT_CRITICAL(&lifecycle_mux); if (blocked) return false;
    if (xTaskCreatePinnedToCoreWithCaps(pair_task,"camera_pair",32768,(void *)(uintptr_t)jpeg,4,NULL,0,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS) {
        portENTER_CRITICAL(&lifecycle_mux); atomic_store(&busy,false); atomic_store(&worker_active,false); portEXIT_CRITICAL(&lifecycle_mux); ESP_LOGE(TAG,"Cannot allocate camera task");
        return false;
    }
    return true;
}
void camera_pair_start(void) { start_request(false); }
void camera_jpeg_start(void) { start_request(true); }
static void request_stop_locked(void) { if (!atomic_load(&stop_requested)) { atomic_store(&stop_ms,now_ms(NULL)); atomic_store(&stop_requested,true); } }
void camera_stop_request(void) { portENTER_CRITICAL(&lifecycle_mux); request_stop_locked(); portEXIT_CRITICAL(&lifecycle_mux); camera_focus_cancel(); }
#if CONFIG_REMOTE_DBG_SIM
bool camera_display_begin(void)
{
    portENTER_CRITICAL(&lifecycle_mux);
    bool allowed=!maintenance_gate && camera_controller_admitting();
    if (allowed) { maintenance_gate=display_gate=true;request_stop_locked(); }
    portEXIT_CRITICAL(&lifecycle_mux);
    if (allowed) camera_focus_cancel();
    return allowed;
}
bool camera_display_ready(void)
{
    portENTER_CRITICAL(&lifecycle_mux);bool ready=display_gate && !atomic_load(&busy);
    portEXIT_CRITICAL(&lifecycle_mux);return ready;
}
void camera_display_end(void)
{
    portENTER_CRITICAL(&lifecycle_mux);
    if (display_gate) { display_gate=false;maintenance_gate=!camera_controller_admitting(); }
    portEXIT_CRITICAL(&lifecycle_mux);
}
#endif
void camera_debug_get_status(camera_debug_status_t *out)
{
    if (!out) return;
    gamepad_caps_t c; camera_gamepad_caps(&c);
    *out=(camera_debug_status_t){.busy=atomic_load(&busy),.stopped=atomic_load(&stop_requested),.session=c.session,.last_io=atomic_load(&last_io)};
    portENTER_CRITICAL(&debug_mux); memcpy(out->phase,debug_phase,sizeof out->phase); portEXIT_CRITICAL(&debug_mux);
}
esp_err_t app_camera_init(const app_camera_config_t *config)
{
    if (!config || config->backend!=APP_CAMERA_BACKEND_DEFAULT) return ESP_ERR_INVALID_ARG;
    if (!camera_controller_admitting()) return ESP_ERR_INVALID_STATE;
    if (initialized) return ESP_ERR_INVALID_STATE;
    stream.controls.enter=enter_controls; stream.controls.leave=leave_controls; camera_controls_session(&stream.controls,false,PAD_LENS_POWER_ZOOM);
    focus_requests=xQueueCreate(1,sizeof(focus_request_t));
    if (!focus_requests) return ESP_ERR_NO_MEM;
    for (unsigned i=0;i<2;++i) {
        packet_buffers[i]=heap_caps_malloc(OBJECT_CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if (!packet_buffers[i]) {
            for (unsigned j=0;j<2;++j) { heap_caps_free(packet_buffers[j]); packet_buffers[j]=NULL; }
            vQueueDelete(focus_requests); focus_requests=NULL;
            return ESP_ERR_NO_MEM;
        }
    }
    ESP_LOGI(TAG,"Boot packet buffers ready: 2 x 512 KiB PSRAM");
    initialized=true; return ESP_OK;
}
esp_err_t app_camera_messages_start(void)
{
    if (!initialized || messages_started || !camera_controller_admitting()) return ESP_ERR_INVALID_STATE;
    esp_err_t error=camera_endpoint_start();
    if (error==ESP_OK) messages_started=true;
    return error;
}
esp_err_t app_camera_start(app_camera_start_mode_t mode)
{
    if (mode!=APP_CAMERA_START_PREVIEW && mode!=APP_CAMERA_START_PAIR) return ESP_ERR_INVALID_ARG;
    if (!initialized || !messages_started) return ESP_ERR_INVALID_STATE;
    return start_request(mode==APP_CAMERA_START_PREVIEW) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
uint32_t app_camera_api_version(void) { return APP_CAMERA_API_VERSION; }
void app_camera_close_admission(void) { atomic_store(&exclusive_closed,true); }
bool app_camera_stop(uint32_t timeout_ms)
{
    app_camera_close_admission();
    portENTER_CRITICAL(&lifecycle_mux);
    if (exclusive_drained) { portEXIT_CRITICAL(&lifecycle_mux);return true; }
    maintenance_gate=true;request_stop_locked();
    portEXIT_CRITICAL(&lifecycle_mux);
    camera_focus_cancel();
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&busy) || atomic_load(&worker_active)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    portENTER_CRITICAL(&lifecycle_mux);exclusive_drained=true;portEXIT_CRITICAL(&lifecycle_mux);
    return true;
}
bool app_camera_messages_quiesce(uint32_t timeout_ms)
{
    portENTER_CRITICAL(&lifecycle_mux); bool gated=maintenance_gate; portEXIT_CRITICAL(&lifecycle_mux);
    if (!messages_started) {
        /* Init may allocate the focus queue before endpoint allocation fails.
         * Physical stop has already drained any owner; no endpoint can free it. */
        if (exclusive_drained && focus_requests) { vQueueDelete(focus_requests);focus_requests=NULL; }
        return true;
    }
    if (camera_controller_active() || !gated) return false;
    if (!camera_endpoint_quiesce(timeout_ms)) return false;
    messages_started=false;
    if (!camera_controller_admitting() && focus_requests) { vQueueDelete(focus_requests);focus_requests=NULL; }
    return true;
}
