#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_input/input_service.c"
static int64_t clock_us=1000;
static uint32_t generations[APP_ENDPOINT_COUNT]={0,1,2,3,4,5,0,7,8};
static bool register_ok=true,create_ok=true,camera_ok=true,cancel_ok=true,change_epoch;
static bool expired_reply;
static bool queued,reply_camera_live=true;
static app_message_t inbox,last_reply,last_command,last_camera,last_menu;
static gamepad_caps_t camera_caps={.session=true,.generation=42};
static app_ui_state_t ui_state;
static app_ui_menu_route_t route;
static unsigned preference=1,commands_sent,camera_calls,menu_calls,endpoint_stops,task_deletes;
static unsigned menu_delay_ms;
static TaskFunction_t worker;
int64_t esp_timer_get_time(void) { return clock_us; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)(clock_us/1000); }
void vTaskDelay(TickType_t delay) { clock_us+=(int64_t)delay*1000; }
void vTaskDelayUntil(TickType_t *previous,TickType_t period)
{ *previous+=period;clock_us=(int64_t)*previous*1000;atomic_store(&stopping,true); }
void vTaskDelete(void *handle) { assert(!handle);++task_deletes; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{ assert(!strcmp(name,"input_owner") && stack==4096 && priority==4 && !context && !handle);worker=fn;return create_ok?pdPASS:pdFALSE; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { return generations[endpoint]; }
esp_err_t app_console_endpoint_register(app_endpoint_t endpoint,const app_endpoint_config_t *config)
{ assert(endpoint==APP_ENDPOINT_INPUT && config->control_depth==8 && config->bulk_depth==1);return register_ok?ESP_OK:ESP_ERR_INVALID_STATE; }
void app_console_endpoint_stop(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_INPUT);++endpoint_stops;++generations[endpoint]; }
void app_message_release(app_message_t *m) { assert(!m->lease); }
esp_err_t app_console_send(app_message_t *m)
{
    assert(m->source==APP_ENDPOINT_INPUT && m->generation==generations[APP_ENDPOINT_INPUT] && !m->lease);
    if (m->type==APP_MESSAGE_UI_MENU_ACTION) {
        assert(!m->flags && !m->deadline_us && m->payload.action.type==PAD_ACTION_RELEASE_ALL);
        return cancel_ok?ESP_OK:ESP_ERR_NO_MEM;
    }
    last_command=*m;++commands_sent;return ESP_OK;
}
esp_err_t app_console_request(app_message_t *m,app_message_t *reply)
{
    assert(m->source==APP_ENDPOINT_INPUT && m->flags==APP_MESSAGE_REQUEST && !m->lease);
    assert(m->generation==generations[APP_ENDPOINT_INPUT]);
    assert(m->deadline_us==clock_us+100000 ||
        (m->type==APP_MESSAGE_UI_MENU_ACTION && m->deadline_us==clock_us+500000));
    *reply=(app_message_t){.result=ESP_OK};
    switch(m->type) {
    case APP_MESSAGE_CAMERA_CAPABILITIES:
        if (!camera_ok) return ESP_ERR_TIMEOUT;
        reply->payload.capabilities=camera_caps;break;
    case APP_MESSAGE_CAMERA_ACTION:
        ++camera_calls;last_camera=*m;
        if (!camera_ok) return ESP_ERR_TIMEOUT;
        if (expired_reply) { reply->result=ESP_ERR_TIMEOUT;break; }
        if (m->payload.action.type==PAD_ACTION_RELEASE_ALL) ++camera_caps.generation;
        camera_caps.session=reply_camera_live;
        reply->payload.capabilities=camera_caps;
        if (!reply_camera_live) reply->result=ESP_ERR_INVALID_STATE;
        break;
    case APP_MESSAGE_UI_STATUS: reply->payload.ui=ui_state;break;
    case APP_MESSAGE_UI_PREFERENCES:
        assert(m->payload.command.index==APP_UI_PREF_GET);
        reply->payload.command.direction=preference;break;
    case APP_MESSAGE_UI_MENU_ACTION:
        ++menu_calls;last_menu=*m;
        clock_us+=(int64_t)menu_delay_ms*1000;
        if(m->deadline_us<=clock_us)return ESP_ERR_TIMEOUT;
        reply->payload.menu=(app_ui_menu_result_t){.route=route,.state=ui_state,.property=APP_CAMERA_PROPERTY_EV};break;
    case APP_MESSAGE_CAMERA_MENU_ACTION: last_camera=*m;break;
    default:assert(false);
    }
    if (change_epoch) ++generations[m->target];
    return ESP_OK;
}
esp_err_t app_console_request_cancelable(app_message_t *m,app_message_t *reply,app_console_cancel_fn fn,void *context)
{ if (fn(context)) return ESP_ERR_INVALID_STATE;return app_console_request(m,reply); }
esp_err_t app_console_receive(app_endpoint_t endpoint,app_message_t *m,uint32_t timeout)
{ assert(endpoint==APP_ENDPOINT_INPUT && !timeout);if(!queued)return ESP_ERR_TIMEOUT;*m=inbox;queued=false;return ESP_OK; }
esp_err_t app_console_reply(const app_message_t *m,app_message_t *reply)
{ assert(m->type==inbox.type);last_reply=*reply;return ESP_OK; }
int main(void)
{
    register_ok=false;assert(app_input_start()==ESP_ERR_INVALID_STATE && !atomic_load(&running));
    register_ok=true;create_ok=false;assert(app_input_start()==ESP_ERR_NO_MEM && !atomic_load(&running) && endpoint_stops==1);
    create_ok=true;assert(app_input_start()==ESP_OK && worker);
    assert(app_input_start()==ESP_ERR_INVALID_STATE);
    input_owner_init(&owner,action,NULL);refresh_camera();
    owner.latest.gimbal_fault=true;assert(snapshot().gimbal_fault);
    owner.latest.gimbal_fault=false;assert(!snapshot().gimbal_fault);
    assert(caps.session && caps.generation==camera_caps.generation);
    ui_state.settings=true;refresh_ui();
    assert(caps.settings && preferences_known && pad_kind==1 && !pad_dirty[0]);
    assert(last_command.type==APP_MESSAGE_INPUT_ATOM_COMMAND && last_command.payload.command.value==1);
    unsigned before=commands_sent;refresh_ui();assert(commands_sent==before);
    ++generations[APP_ENDPOINT_INPUT_ATOM];refresh_ui();assert(commands_sent==before+1);
    generations[APP_ENDPOINT_INPUT_SIM]=6;refresh_ui();
#if CONFIG_REMOTE_DBG_SIM
    assert(commands_sent==before+2);
    assert(last_command.type==APP_MESSAGE_INPUT_SIM_COMMAND &&
        last_command.payload.command.index==APP_INPUT_SIM_PAD_KIND && last_command.payload.command.value==1);
#else
    assert(commands_sent==before+1 && last_command.type==APP_MESSAGE_INPUT_ATOM_COMMAND);
#endif
    before=commands_sent;
    assert(action(NULL,(pad_action_t){.type=PAD_ACTION_UI_INFO_NEXT}) && commands_sent==before);
    /* UI's sole consumer may finish a real JPEG (~200-300ms) first. A menu
     * action must remain admitted without enlarging Camera's safety budget. */
    menu_delay_ms=250;
    assert(action(NULL,(pad_action_t){.type=PAD_ACTION_UI_TOGGLE}));
    menu_delay_ms=0;
    route=APP_UI_MENU_CAMERA;uint32_t safety_gen=caps.generation;
    assert(action(NULL,(pad_action_t){PAD_ACTION_MENU_STEP,1,safety_gen}));
    assert(last_camera.type==APP_MESSAGE_CAMERA_MENU_ACTION && last_camera.payload.command.index==APP_CAMERA_PROPERTY_EV &&
        last_camera.payload.command.token==safety_gen && last_camera.payload.command.direction==1);
    route=APP_UI_MENU_WIFI;before=camera_calls;
    assert(action(NULL,(pad_action_t){PAD_ACTION_MENU_CONFIRM,0,safety_gen}));assert(camera_calls==before);
    assert(action(NULL,(pad_action_t){PAD_ACTION_MAINT_TOGGLE,0,safety_gen}) && camera_calls==before);
    cancel_ok=false;
    assert(action(NULL,(pad_action_t){PAD_ACTION_RELEASE_ALL,0,1}));
    assert(cancel_pending && last_camera.payload.action.generation==safety_gen && caps.generation==safety_gen+1);
    cancel_ok=true;flush_cancel();assert(!cancel_pending);
    camera_ok=false;assert(!action(NULL,(pad_action_t){PAD_ACTION_RELEASE_ALL,0,1}) && !caps.session);
    camera_ok=true;refresh_camera();assert(caps.session);
    expired_reply=true;
    assert(!action(NULL,(pad_action_t){PAD_ACTION_RELEASE_ALL,0,1}));
    expired_reply=false;refresh_camera();
    change_epoch=true;assert(!action(NULL,(pad_action_t){PAD_ACTION_RECORD,1,caps.generation}));
    change_epoch=false;refresh_camera();
    input_provider_handle_t handle;
    assert(input_provider_register(INPUT_SOURCE_ATOM,&handle)==ESP_OK);
    input_report_t r={.connected=true,.atom_online=true,.source_epoch=1,.report_id=1,.battery=5};
    assert(input_provider_publish(handle,&r)==ESP_OK);input_owner_tick(&owner,&caps,now_ms());
    assert(snapshot().connected && snapshot().battery==5);
    inbox=(app_message_t){.type=APP_MESSAGE_INPUT_SELECT,.source=APP_ENDPOINT_UI,.target=APP_ENDPOINT_INPUT,
        .generation=generations[APP_ENDPOINT_UI],.endpoint_epoch=generations[APP_ENDPOINT_INPUT],
        .flags=APP_MESSAGE_REQUEST,.deadline_us=clock_us+1000};inbox.payload.command.index=1;
    queued=true;receive();assert(last_reply.result==ESP_ERR_INVALID_ARG && owner.selected==INPUT_SOURCE_ATOM);
    inbox.source=APP_ENDPOINT_UART;inbox.generation=generations[APP_ENDPOINT_UART];queued=true;receive();
#if CONFIG_REMOTE_DBG_SIM
    assert(last_reply.result==ESP_OK && owner.selected==INPUT_SOURCE_UART_SIM && !snapshot().connected);
#else
    assert(last_reply.result==ESP_ERR_NOT_SUPPORTED && owner.selected==INPUT_SOURCE_ATOM && snapshot().connected);
#endif
    inbox.endpoint_epoch--;queued=true;receive();assert(last_reply.result==ESP_ERR_INVALID_ARG);
    atomic_store(&stopping,true);
    assert(!action(NULL,(pad_action_t){PAD_ACTION_RECORD,1,caps.generation}));
    assert(action(NULL,(pad_action_t){PAD_ACTION_RELEASE_ALL,0,caps.generation}));
    assert(app_input_quiesce(0)==ESP_ERR_TIMEOUT && atomic_load(&running));
    reply_camera_live=false;worker(NULL);
    assert(!atomic_load(&running) && task_deletes==1 && endpoint_stops==2);
    assert(input_provider_publish(handle,&r)==ESP_ERR_INVALID_STATE);
    assert(app_input_start()==ESP_ERR_INVALID_STATE); /* Provider still registered. */
    assert(input_provider_unregister(handle)==ESP_OK);
    assert(app_input_start()==ESP_OK);worker(NULL);assert(!atomic_load(&running));
    assert(app_input_quiesce(0)==ESP_OK);
    puts("Input service resource bounds, typed dispatch, safety retries, epochs and stop/restart passed");
}
