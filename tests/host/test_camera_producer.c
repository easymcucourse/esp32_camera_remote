#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../components/app_camera/camera_runtime.c"
#include "../support/legacy/camera_runtime_obsolete.c"
struct fake_queue { focus_request_t item; bool full; };
static TaskFunction_t owner_task;
static void *owner_argument;
static unsigned creates,connects,destroys,confirms,actions,objects,allocations,cleanup_retries;
static bool backend_owned,connected,fail_close,load_ok=true,fail_alloc;
static bool network_during_read;
static unsigned network_notifications;
static camera_backend_result_t connect_result=CAMERA_BACKEND_OK;
static uint32_t clock_ms;
static app_message_t held[2],metadata[2];
static unsigned held_count,metadata_count;
static uint32_t latest_state_generation;
static camera_backend_t fake_backend;
static camera_backend_result_t connect_camera(void *context,const camera_connection_t *c,void *scratch,size_t capacity,uint32_t timeout,camera_peer_t *peer)
{
    assert(context && scratch && capacity==OBJECT_CAPACITY && timeout==50000 && c->port==15740 && c->handshake_timeout_ms==10000 && c->require_peer_guid);
    ++connects; connected=connect_result==CAMERA_BACKEND_OK;
    *peer=(camera_peer_t){0}; peer->guid[0]=2; strcpy(peer->model,"fake model"); return connect_result;
}
static camera_backend_result_t disconnect_camera(void *context,uint32_t timeout)
{
    assert(context && timeout==1000);
    if (connected && fail_close) { fail_close=false; ++cleanup_retries; return CAMERA_BACKEND_NETWORK; }
    connected=false; return CAMERA_BACKEND_OK;
}
static void cancel_camera(void *context) { assert(context); }
static void network_camera(void *context,uint32_t generation) { assert(context && generation); ++network_notifications; }
static camera_backend_result_t destroy_camera(void *context)
{ assert(context && backend_owned && !connected); backend_owned=false; ++destroys; return CAMERA_BACKEND_OK; }
static camera_backend_result_t properties_camera(void *context,void *scratch,size_t capacity,uint32_t timeout,camera_property_visitor_t visitor,void *target,camera_capabilities_t *caps)
{
    assert(context && scratch && capacity==OBJECT_CAPACITY && timeout);
    if (network_during_read) { network_during_read=false; camera_controller_network_changed(5); }
    static const camera_value_t choices[]={{CAMERA_VALUE_U32,1},{CAMERA_VALUE_U32,2}};
    camera_property_t property={.setting=CAMERA_SETTING_MODE,.current={CAMERA_VALUE_U32,1},.writable=true,.choices=choices,.choice_count=2};
    *caps=(camera_capabilities_t){.recording_known=true}; visitor(target,&property); return CAMERA_BACKEND_OK;
}
static camera_backend_result_t set_camera(void *context,camera_setting_t setting,camera_value_t value,uint32_t timeout)
{ (void)context;(void)setting;(void)value;(void)timeout; assert(false); return CAMERA_BACKEND_OK; }
static camera_backend_result_t step_camera(void *context,camera_setting_t setting,int direction,uint32_t timeout)
{ (void)context;(void)setting;(void)direction;(void)timeout; assert(false); return CAMERA_BACKEND_OK; }
static camera_backend_result_t object_camera(void *context,void *scratch,size_t capacity,uint32_t timeout,camera_frame_t *frame)
{ assert(context && scratch && capacity==OBJECT_CAPACITY && timeout==5000); if (cancelled(NULL)) return CAMERA_BACKEND_CANCELLED; ++objects; *frame=(camera_frame_t){scratch,8}; return CAMERA_BACKEND_OK; }
static camera_backend_result_t action_camera(void *context,camera_action_t action,int value,uint32_t timeout)
{
    assert(context && timeout && timeout<=5000); ++actions;
    assert(action==CAMERA_ACTION_SHUTTER_HALF || action==CAMERA_ACTION_SHUTTER_FULL);
    if (!value) assert(held_count==2); /* Physical release precedes JPEG lease drain. */
    return CAMERA_BACKEND_OK;
}
static camera_backend_result_t events_camera(void *context,uint32_t timeout,bool *changed)
{ assert(context && timeout==5000); *changed=false; return CAMERA_BACKEND_OK; }
static camera_backend_result_t probe_camera(void *context,const char *address,uint16_t port,uint32_t timeout)
{ assert(context && !strcmp(address,"192.0.2.1") && port==15740 && timeout==800); return CAMERA_BACKEND_OK; }
static const camera_backend_ops_t ops={.api_version=1,.capabilities=31,.connect=connect_camera,.disconnect=disconnect_camera,.cancel=cancel_camera,.network_changed=network_camera,
    .destroy=destroy_camera,.properties=properties_camera,.set=set_camera,.step=step_camera,.liveview=object_camera,.action=action_camera,.events=events_camera,.probe=probe_camera};
static camera_backend_result_t create_camera(uint32_t generation,bool (*predicate)(void *),void *context,camera_backend_t **backend)
{
    assert(generation && predicate && !context && !backend_owned); ++creates; backend_owned=true;
    fake_backend=(camera_backend_t){.api_version=1,.capabilities=31,.context=&fake_backend,.ops=&ops}; *backend=&fake_backend; return CAMERA_BACKEND_OK;
}
const camera_backend_factory_t camera_backend_default_factory={.api_version=1,.required_capabilities=31,.create=create_camera};
bool camera_identity_run(camera_identity_t *identity,camera_identity_operation_t operation,const uint8_t mac[6],const uint8_t guid[16])
{
    if (operation==CAMERA_IDENTITY_LOAD) { if (!load_ok) return false; *identity=(camera_identity_t){.paired=true}; identity->guid[0]=1; identity->peer[0]=3; identity->peer[6]=2; }
    if (operation==CAMERA_IDENTITY_CONFIRM) { assert(connected && identity && mac[0]==3 && guid[0]==2); ++confirms; }
    return true;
}
void *heap_caps_malloc(size_t size,unsigned caps)
{ assert(size==OBJECT_CAPACITY && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)); if (fail_alloc) return NULL; ++allocations; return malloc(size); }
void heap_caps_free(void *pointer) { if (pointer) { --allocations; free(pointer); } }
void *test_output_calloc(size_t n,size_t size) { return calloc(n,size); }
void test_output_free(void *pointer) { free(pointer); }
int64_t esp_timer_get_time(void) { return (int64_t)clock_ms*1000; }
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t function,const char *name,unsigned stack,void *context,unsigned priority,void *handle,unsigned core,unsigned caps)
{ assert(!strcmp(name,"camera_pair") && stack==32768 && priority==4 && !handle && !core && caps==3); owner_task=function; owner_argument=context; return pdPASS; }
int xPortGetCoreID(void) { return 0; }
void vTaskDeleteWithCaps(void *task) { assert(!task && !backend_owned && allocations==2 && !held_count); }
void vTaskDelay(TickType_t delay)
{
    clock_ms+=delay;
    if (clock_ms>30000) {
        fprintf(stderr,"producer stalled: phase=%s creates=%u connects=%u frames=%u held=%u actions=%u result=%d stopped=%d\n",debug_phase,creates,connects,objects,held_count,actions,atomic_load(&last_io),atomic_load(&stop_requested));
        assert(false);
    }
    if (held_count==2 && atomic_load(&stop_requested) && delay==10) {
        assert(actions==4);
        for (unsigned i=0;i<2;++i) {
            metadata[i]=(app_message_t){.type=APP_MESSAGE_UI_FRAME_RESULT,.source=APP_ENDPOINT_UI,.generation=held[i].generation,.result=ESP_OK};
            metadata[i].payload.command.token=held[i].payload.command.token; app_message_release(&held[i]);
        }
        held_count=0; metadata_count=2;
    }
}
QueueHandle_t xQueueCreate(unsigned capacity,size_t size)
{ assert(capacity==1 && size==sizeof(focus_request_t)); return calloc(1,sizeof(struct fake_queue)); }
void vQueueDelete(QueueHandle_t queue) { assert(queue);free(queue); }
BaseType_t xQueueReset(QueueHandle_t queue) { queue->full=false; return pdTRUE; }
BaseType_t xQueueOverwrite(QueueHandle_t queue,const void *item) { queue->item=*(const focus_request_t *)item; queue->full=true; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t timeout)
{ assert(!timeout); if (!queue->full) return pdFALSE; *(focus_request_t *)item=queue->item; queue->full=false; return pdTRUE; }
static bool endpoint_start_ok=true;
esp_err_t camera_endpoint_start(void) { return endpoint_start_ok?ESP_OK:ESP_ERR_NO_MEM; }
void camera_endpoint_stop(void) {}
static bool endpoint_stop_ok=true;
bool camera_endpoint_quiesce(uint32_t timeout) { (void)timeout;return endpoint_stop_ok; }
bool camera_endpoint_frame_result(app_message_t *message)
{ if (!metadata_count) return false; *message=metadata[--metadata_count]; return true; }
void camera_console_init(void) {}
esp_err_t app_console_request_cancelable(app_message_t *request,app_message_t *reply,app_console_cancel_fn predicate,void *context)
{
    assert(request->type==APP_MESSAGE_CAMERA_DISCOVER && request->source==APP_ENDPOINT_CAMERA && request->target==APP_ENDPOINT_WIFI && predicate && !context);
    reply->result=ESP_OK; reply->payload.discovery.count=1; reply->payload.discovery.clients[0].mac[0]=3; strcpy(reply->payload.discovery.clients[0].ip,"192.0.2.1"); return ESP_OK;
}
esp_err_t app_console_request(app_message_t *request,app_message_t *reply)
{
    assert(request->generation);
    if (request->type==APP_MESSAGE_SYSTEM_CAMERA_SESSION) {
        assert(request->source==APP_ENDPOINT_CAMERA && request->target==APP_ENDPOINT_SYSTEM);
        if (request->payload.command.flag) assert(connected && confirms);
    } else assert(request->type==APP_MESSAGE_WIFI_SELECT_CAMERA);
    reply->result=ESP_OK; return ESP_OK;
}
esp_err_t app_console_send(app_message_t *message)
{
    if (message->type==APP_MESSAGE_CAMERA_FRAME) {
        assert(held_count<2); held[held_count++]=*message; message->lease=NULL;
        if (held_count==1) {
            gamepad_caps_t caps; camera_gamepad_caps(&caps);
            assert(camera_gamepad_action((pad_action_t){PAD_ACTION_S1,1,caps.generation}));
            assert(camera_gamepad_action((pad_action_t){PAD_ACTION_S2,1,caps.generation}));
        } else camera_stop_request();
    } else {
        if (message->type==APP_MESSAGE_CAMERA_STATE) { assert(message->generation>=latest_state_generation); latest_state_generation=message->generation; }
        app_message_release(message);
    }
    return ESP_OK;
}
static void run(bool jpeg)
{
    if (jpeg) camera_jpeg_start(); else camera_pair_start();
    assert(camera_controller_active() && atomic_load(&busy)); owner_task(owner_argument);
    assert(!camera_controller_active() && !atomic_load(&busy) && !backend_owned && allocations==2 && !camera_controller_frame_generation());
}
int main(int argc,char **argv)
{
    const app_camera_config_t config={.backend=APP_CAMERA_BACKEND_DEFAULT};
    assert(app_camera_start(APP_CAMERA_START_PREVIEW)==ESP_ERR_INVALID_STATE);
    fail_alloc=true;
    assert(app_camera_init(NULL)==ESP_ERR_INVALID_ARG && app_camera_init(&config)==ESP_ERR_NO_MEM);
    assert(!allocations && !focus_requests && !packet_buffers[0] && !packet_buffers[1]);
    fail_alloc=false;
    assert(app_camera_init(&config)==ESP_OK && allocations==2 && OBJECT_CAPACITY==512u*1024u);
    assert(app_camera_init(&config)==ESP_ERR_INVALID_STATE);
    if(argc==2) {
        assert(!strcmp(argv[1],"partial") && focus_requests);
        endpoint_start_ok=false;
        assert(app_camera_messages_start()==ESP_ERR_NO_MEM && !messages_started && focus_requests);
        assert(app_camera_stop(0) && app_camera_messages_quiesce(0) && !focus_requests);
        assert(app_camera_messages_quiesce(0) && app_camera_messages_start()==ESP_ERR_INVALID_STATE);
        return 0;
    }
    assert(argc==1);
    assert(app_camera_messages_start()==ESP_OK && app_camera_messages_start()==ESP_ERR_INVALID_STATE);
    fail_close=true; run(true);
    assert(connects==1 && confirms==1 && creates==destroys && actions==4 && objects==2 && cleanup_retries==1);
    unsigned before=connects; connect_result=CAMERA_BACKEND_IDENTITY; run(true);
    assert(connects==before+1 && confirms==1 && creates==destroys); /* No automatic InitFail retry. */
    connect_result=CAMERA_BACKEND_OK; before=connects; run(false);
    assert(connects==before+2 && confirms==3 && creates==destroys); /* 300ms paired reconnect verification. */
    before=connects; load_ok=false; run(true); assert(connects==before && creates==destroys);
    load_ok=true; fail_alloc=true; actions=objects=0; run(true); assert(connects==before+1 && allocations==2);
    fail_alloc=false; actions=objects=0; before=connects; network_during_read=true; run(true);
    assert(connects==before+2 && network_notifications==1 && actions==4 && objects==2 && creates==destroys);
    assert(!camera_controller_forget(true));
#if CONFIG_REMOTE_DBG_SIM
    assert(camera_display_begin() && camera_display_ready() && !camera_display_begin());
    assert(app_camera_start(APP_CAMERA_START_PREVIEW)==ESP_ERR_INVALID_STATE && !camera_controller_forget(false));
    assert(!app_camera_quiesce(0));camera_display_end();assert(!camera_display_ready());
    actions=objects=0;run(true);assert(!backend_owned && allocations==2);
#endif
    assert(!app_camera_messages_quiesce(0) && atomic_load(&messages_started));
    assert(app_camera_quiesce(1000) && camera_controller_forget(true) && atomic_load(&busy)); app_camera_messages_stop();
    assert(app_camera_start(APP_CAMERA_START_PREVIEW)==ESP_ERR_INVALID_STATE);
    assert(app_camera_messages_start()==ESP_OK);
    endpoint_stop_ok=false;
    assert(!app_camera_messages_quiesce(0) && atomic_load(&messages_started));
    assert(app_camera_messages_start()==ESP_ERR_INVALID_STATE);
    endpoint_stop_ok=true;
    assert(app_camera_messages_quiesce(10) && !atomic_load(&messages_started));
    assert(app_camera_messages_quiesce(0));
    app_camera_quiesce_release();assert(app_camera_messages_start()==ESP_OK);
    camera_controls_session(&stream.controls,true,PAD_LENS_POWER_ZOOM);
    atomic_store(&ready,true);atomic_store(&stop_requested,false);
    gamepad_caps_t safe_caps;camera_gamepad_caps(&safe_caps);
    app_camera_close_admission();
    assert(!camera_controller_admitting() && app_camera_start(APP_CAMERA_START_PREVIEW)==ESP_ERR_INVALID_STATE);
    assert(!camera_gamepad_action((pad_action_t){PAD_ACTION_S1,1,safe_caps.generation}));
    assert(camera_gamepad_action((pad_action_t){PAD_ACTION_RELEASE_ALL,0,safe_caps.generation}));
#if CONFIG_REMOTE_DBG_SIM
    assert(!camera_display_begin());
#endif
    atomic_store(&busy,true);atomic_store(&worker_active,true);
    uint32_t stop_at=clock_ms;
    assert(!app_camera_stop(20) && clock_ms-stop_at==20 && maintenance_gate && !camera_controller_admitting());
    app_camera_quiesce_release();assert(maintenance_gate && !camera_controller_admitting());
    assert(!app_camera_stop(0)); /* A physical worker still owns cleanup. */
    atomic_store(&worker_active,false);
    assert(app_camera_stop(0) && app_camera_stop(0));
    assert(app_camera_messages_quiesce(0) && !focus_requests && app_camera_messages_start()==ESP_ERR_INVALID_STATE);
    assert(app_camera_start(APP_CAMERA_START_PREVIEW)==ESP_ERR_INVALID_STATE);
    for(unsigned i=0;i<2;++i) heap_caps_free(packet_buffers[i]);
    free(focus_requests); return 0;
}
