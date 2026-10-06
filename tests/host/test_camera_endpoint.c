#include <assert.h>
#include <stdlib.h>
#include <string.h>
/* Compile the production intake and drive its handlers while the fake owner
 * stays blocked. No network completion is needed for STOP admission/status. */
#include "../../components/app_camera/camera_endpoint.c"
struct fake_queue { app_message_t entries[4]; unsigned head,count; };
static bool active,forget_ok=true,create_ok=true;
static bool register_ok=true;
static unsigned endpoint_stops;
static bool router_frozen;
static unsigned subscribed;
void app_console_get_status(app_console_status_t *out)
{ *out=(app_console_status_t){.running=true,.accepting=true,.subscriptions_frozen=router_frozen}; }
static uint32_t announced_network;
static uint32_t lifetime=1,frame_gen=9;
static int64_t clock_us=100;
static unsigned cancelled,replied,forgotten,starts,settings,releases;
static app_message_t last_reply;
#if CONFIG_REMOTE_DBG_SIM
static bool display_gate;
static uint32_t ui_epoch=4;
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ return endpoint==APP_ENDPOINT_UI ? ui_epoch : 5; }
bool camera_display_begin(void) { if (display_gate) return false;display_gate=true;camera_stop_request();return true; }
bool camera_display_ready(void) { return display_gate && !active; }
void camera_display_end(void) { display_gate=false; }
#endif
static TaskFunction_t task;
bool camera_controller_active(void) { return active; }
static bool admitting=true;
bool camera_controller_admitting(void) { return admitting; }
uint32_t camera_controller_lifetime(void) { return lifetime; }
uint32_t camera_controller_frame_generation(void) { return frame_gen; }
void camera_controller_network_changed(uint32_t generation) { announced_network=generation; }
void camera_controller_setting(unsigned property,int direction)
{ assert(property==APP_CAMERA_PROPERTY_EV && direction==1); ++settings; }
void camera_stop_request(void) { ++cancelled; }
void camera_pair_start(void) {
#if CONFIG_REMOTE_DBG_SIM
    if (display_gate) return;
#endif
    ++starts; active=true; ++lifetime;
}
void camera_jpeg_start(void) { camera_pair_start(); }
bool camera_forget_pairing(void) { ++forgotten; return forget_ok; }
bool camera_controller_forget(bool reserved) { assert(reserved); ++forgotten; return forget_ok; }
bool camera_gamepad_action(pad_action_t action) { return action.generation==42; }
void camera_gamepad_caps(gamepad_caps_t *out) { *out=(gamepad_caps_t){.session=active,.generation=42}; }
void camera_debug_get_status(camera_debug_status_t *out)
{ *out=(camera_debug_status_t){.busy=active,.session=active,.last_io=7}; strcpy(out->phase,"waiting IO"); }
int64_t esp_timer_get_time(void) { return clock_us; }
esp_err_t app_console_endpoint_register(app_endpoint_t endpoint,const app_endpoint_config_t *config)
{ assert(endpoint==APP_ENDPOINT_CAMERA && config->control_depth==16 && config->bulk_depth==2); return register_ok ? ESP_OK : ESP_ERR_INVALID_STATE; }
void app_console_endpoint_stop(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_CAMERA); ++endpoint_stops; }
esp_err_t app_console_subscribe(app_message_type_t type,app_endpoint_t endpoint)
{ assert(type==APP_MESSAGE_WIFI_NETWORK_CHANGED && endpoint==APP_ENDPOINT_CAMERA && !router_frozen);++subscribed; return ESP_OK; }
esp_err_t app_console_receive(app_endpoint_t endpoint,app_message_t *message,uint32_t timeout)
{ (void)endpoint;(void)message;(void)timeout; return ESP_ERR_INVALID_STATE; }
esp_err_t app_console_reply(const app_message_t *request,app_message_t *reply)
{ assert(request->type==APP_MESSAGE_CAMERA_STOP || request->type==APP_MESSAGE_CAMERA_DISPLAY_SESSION); ++replied; last_reply=*reply; return ESP_OK; }
void app_message_release(app_message_t *message) { assert(!message->lease); ++releases; }
BaseType_t xTaskCreate(TaskFunction_t function,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{ assert(!strcmp(name,"camera_endpoint") && stack==4096 && priority==4 && !context && !handle); task=function; return create_ok ? pdPASS : pdFALSE; }
void vTaskDelete(void *handle) { assert(!handle); }
void vTaskDelay(TickType_t delay) { clock_us+=(int64_t)delay*1000; }
QueueHandle_t xQueueCreate(unsigned capacity,size_t size)
{ assert(capacity==4 && size==sizeof(app_message_t)); return calloc(1,sizeof(struct fake_queue)); }
void vQueueDelete(QueueHandle_t queue) { free(queue); }
BaseType_t xQueueSend(QueueHandle_t queue,const void *item,TickType_t timeout)
{ assert(!timeout); if (queue->count==4) return pdFALSE; queue->entries[(queue->head+queue->count++)%4]=*(const app_message_t *)item; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t queue,void *item,TickType_t timeout)
{ assert(!timeout); if (!queue->count) return pdFALSE; *(app_message_t *)item=queue->entries[queue->head]; queue->head=(queue->head+1)%4; --queue->count; return pdTRUE; }
static esp_err_t dispatch(app_message_t *message,app_message_t *reply)
{ bool deferred=false; return handle(message,reply,&deferred); }
int main(void)
{
    register_ok=false; assert(camera_endpoint_start()==ESP_ERR_INVALID_STATE && !results && !endpoint_stops);
    register_ok=true; create_ok=false; assert(camera_endpoint_start()==ESP_ERR_NO_MEM && !results && endpoint_stops==1);
    create_ok=true; assert(camera_endpoint_start()==ESP_OK && task && camera_endpoint_start()==ESP_ERR_INVALID_STATE);
    app_message_t stop={.type=APP_MESSAGE_CAMERA_STOP,.source=APP_ENDPOINT_SYSTEM,
        .flags=APP_MESSAGE_REQUEST,.generation=1,.deadline_us=10000},reply={0};
    active=true; bool deferred=false;
    app_message_t serial_stop=stop;serial_stop.source=APP_ENDPOINT_UART;serial_stop.payload.command.flag=true;
    assert(handle(&serial_stop,&reply,&deferred)==ESP_OK && !deferred && cancelled==1 && !stop_count);
    serial_stop.source=APP_ENDPOINT_SYSTEM;
    assert(handle(&serial_stop,&reply,&deferred)==ESP_ERR_INVALID_ARG && cancelled==1);
    serial_stop.source=APP_ENDPOINT_UART;serial_stop.flags=0;
    assert(handle(&serial_stop,&reply,&deferred)==ESP_ERR_INVALID_ARG && cancelled==1);
    cancelled=0;
    assert(handle(&stop,&reply,&deferred)==ESP_OK && deferred && cancelled==1 && stop_count==1);
    finish_stops(); assert(!replied && stop_count==1);
    app_message_t query={.type=APP_MESSAGE_CAMERA_STATUS};
    assert(dispatch(&query,&reply)==ESP_OK && reply.payload.camera.busy && reply.payload.camera.last_io==7);
    app_message_t start={.type=APP_MESSAGE_CAMERA_START};
    assert(dispatch(&start,&reply)==ESP_ERR_INVALID_STATE && !starts);
    active=false; finish_stops(); assert(replied==1 && last_reply.result==ESP_OK && !stop_count);
    active=true; deferred=false; assert(handle(&stop,&reply,&deferred)==ESP_OK && deferred);
    clock_us=10000; finish_stops(); assert(replied==1 && !stop_count);
    assert(dispatch(&stop,&reply)==ESP_ERR_TIMEOUT && cancelled==2);
    clock_us=100; deferred=false; assert(handle(&stop,&reply,&deferred)==ESP_OK && deferred);
    ++lifetime; finish_stops(); assert(replied==2 && !stop_count); /* Finished old owner, newly started lifetime. */
    for (unsigned i=0;i<APP_CONSOLE_PENDING_CAPACITY;++i) {
        stop.correlation_id=i+1; deferred=false;
        assert(handle(&stop,&reply,&deferred)==ESP_OK && deferred);
    }
    deferred=false; assert(handle(&stop,&reply,&deferred)==ESP_ERR_NO_MEM && !deferred);
    active=false; finish_stops(); assert(!stop_count && replied==2+APP_CONSOLE_PENDING_CAPACITY);
    active=true;
    app_message_t invalid=stop; invalid.lease=(app_message_lease_t *)(uintptr_t)1;
    unsigned previous=cancelled; assert(dispatch(&invalid,&reply)==ESP_ERR_INVALID_ARG && cancelled==previous);
    app_message_t frame={.type=APP_MESSAGE_UI_FRAME_RESULT,.source=APP_ENDPOINT_UI,.generation=9};
    frame.payload.command.token=12;
    assert(dispatch(&frame,&reply)==ESP_OK); app_message_t metadata;
    assert(camera_endpoint_frame_result(&metadata) && metadata.payload.command.token==12 && !metadata.lease);
    frame.generation=8; assert(dispatch(&frame,&reply)==ESP_ERR_INVALID_STATE);
    frame.generation=9; frame.source=APP_ENDPOINT_SYSTEM; assert(dispatch(&frame,&reply)==ESP_ERR_INVALID_ARG);
    frame.source=APP_ENDPOINT_UI; frame.flags=APP_MESSAGE_REQUEST; assert(dispatch(&frame,&reply)==ESP_ERR_TIMEOUT);
    app_message_t edit={.type=APP_MESSAGE_CAMERA_SETTING_ADJUST};
    edit.payload.command.index=APP_CAMERA_PROPERTY_EV; edit.payload.command.direction=1; edit.payload.command.token=41;
    assert(dispatch(&edit,&reply)==ESP_ERR_INVALID_STATE && !settings);
    edit.payload.command.token=42; assert(dispatch(&edit,&reply)==ESP_OK && settings==1);
    edit.type=APP_MESSAGE_CAMERA_MENU_ACTION;
    assert(dispatch(&edit,&reply)==ESP_OK && settings==2);
    admitting=false;assert(dispatch(&edit,&reply)==ESP_ERR_INVALID_STATE && settings==2);
    admitting=true;
    app_message_t action={.type=APP_MESSAGE_CAMERA_ACTION};
    action.payload.action.generation=41;
    assert(dispatch(&action,&reply)==ESP_ERR_INVALID_STATE && reply.payload.capabilities.generation==42);
    action.payload.action.generation=42;
    assert(dispatch(&action,&reply)==ESP_OK && reply.payload.capabilities.session);
    edit.payload.command.index=APP_CAMERA_PROPERTY_BATTERY; assert(dispatch(&edit,&reply)==ESP_ERR_INVALID_ARG);
    app_message_t forget={.type=APP_MESSAGE_CAMERA_FORGET}; forget_ok=false;
    assert(dispatch(&forget,&reply)==ESP_ERR_NOT_SUPPORTED && !forgotten);
    forget.payload.command.flag=true; forget.source=APP_ENDPOINT_UART;
    assert(dispatch(&forget,&reply)==ESP_ERR_NOT_SUPPORTED && !forgotten);
    forget.source=APP_ENDPOINT_SYSTEM; forget.flags=APP_MESSAGE_REQUEST; forget.deadline_us=clock_us+10000; forget_ok=true;
    assert(dispatch(&forget,&reply)==ESP_ERR_NOT_SUPPORTED && !forgotten);
    active=false; assert(dispatch(&start,&reply)==ESP_OK && starts==1 && active);
    app_message_t network={.type=APP_MESSAGE_WIFI_NETWORK_CHANGED,.source=APP_ENDPOINT_WIFI};
    network.payload.network.generation=5; assert(dispatch(&network,&reply)==ESP_OK && announced_network==5);
    network.source=APP_ENDPOINT_SYSTEM; assert(dispatch(&network,&reply)==ESP_ERR_INVALID_ARG);
    app_message_t display={.type=APP_MESSAGE_CAMERA_DISPLAY_SESSION,.source=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=4,.endpoint_epoch=5,.deadline_us=clock_us+10000,
        .payload.command={.index=1,.token=77}};
#if CONFIG_REMOTE_DBG_SIM
    deferred=false;assert(handle(&display,&reply,&deferred)==ESP_OK && deferred && display_gate);
    unsigned old_replied=replied;finish_display();assert(replied==old_replied);
    active=false;finish_display();assert(replied==old_replied+1 && last_reply.payload.command.token==77 && last_reply.payload.command.flag);
    assert(dispatch(&start,&reply)==ESP_ERR_INVALID_STATE && !active);
    display.payload.command.index=0;display.payload.command.token=78;
    assert(dispatch(&display,&reply)==ESP_ERR_INVALID_STATE && display_gate);
    display.payload.command.token=77;display.payload.command.flag=true;
    assert(dispatch(&display,&reply)==ESP_OK && !display_gate && active);
    display.payload.command.index=1;display.payload.command.flag=false;deferred=false;
    assert(handle(&display,&reply,&deferred)==ESP_OK && deferred);++ui_epoch;finish_display();assert(!display_gate && !display_session.token);
    display.generation=ui_epoch;deferred=false;
    assert(handle(&display,&reply,&deferred)==ESP_OK && deferred);clock_us=display.deadline_us;finish_display();assert(display_gate && display_session.token);
    display.deadline_us=clock_us+10000;display.payload.command.index=0;display.payload.command.flag=true;deferred=false;
    assert(handle(&display,&reply,&deferred)==ESP_OK && deferred && display_gate);
    active=false;finish_display();assert(!display_gate && !display_session.token && active);
#else
    assert(dispatch(&display,&reply)==ESP_ERR_NOT_SUPPORTED);
#endif
    assert(!camera_endpoint_quiesce(0) && results);
    int64_t stop_begin=clock_us;
    assert(!camera_endpoint_quiesce(20) && results && clock_us-stop_begin==20000);
    assert(camera_endpoint_start()==ESP_ERR_INVALID_STATE);
    app_message_t retained={0};assert(xQueueSend(results,&retained,0)==pdTRUE);
    task(NULL); assert(!stop_count); camera_endpoint_stop(); assert(!results);
    assert(camera_endpoint_quiesce(0));
    unsigned before_subscribed=subscribed;
    router_frozen=true;
    assert(camera_endpoint_start()==ESP_OK && subscribed==before_subscribed);
    task(NULL);assert(camera_endpoint_quiesce(0) && !results);
    router_frozen=false;
    assert(camera_endpoint_start()==ESP_OK && subscribed==before_subscribed+1);
    task(NULL);assert(camera_endpoint_quiesce(0));
    return 0;
}
