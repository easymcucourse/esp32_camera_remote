#include "app_core_mode.h"
#include "app_core.h"
#include "app_core_services.h"
#include "app_core_messages.h"
#include "app_console.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static app_message_t inbox,reply;
static bool queued;
static unsigned requested,queried,released;
static unsigned system_stops,router_stops;
static bool router_stop_ok;
static unsigned failure,cleanup_router;
static char cleanup[8];static unsigned cleanup_at;
esp_err_t app_input_quiesce(uint32_t timeout) { assert(timeout==1000);cleanup[cleanup_at++]='I';return failure==3?ESP_ERR_TIMEOUT:ESP_OK; }
bool app_ui_preferences_quiesce(uint32_t timeout) { assert(timeout==1000);cleanup[cleanup_at++]='P';return failure!=4; }
bool app_ui_messages_quiesce(uint32_t timeout) { assert(timeout==1000);cleanup[cleanup_at++]='U';return failure!=5; }
esp_err_t app_console_router_start(void) { return ESP_OK; }
esp_err_t app_console_endpoint_register(app_endpoint_t e,const app_endpoint_config_t *c)
{ assert(e==APP_ENDPOINT_SYSTEM && c->control_depth==8);return ESP_OK; }
esp_err_t app_ui_messages_start(void) { return failure==1?ESP_ERR_NO_MEM:ESP_OK; }
esp_err_t app_input_start(void) { return failure>=2?ESP_ERR_NO_MEM:ESP_OK; }
bool app_console_router_quiesce(uint32_t timeout)
{ if(timeout==1000){assert(!strcmp(cleanup,"IPU") && failure<3);++cleanup_router;return true;}assert(timeout==50 && system_stops==router_stops+1);++router_stops;return router_stop_ok; }
void app_console_endpoint_stop(app_endpoint_t endpoint)
{ assert(endpoint==APP_ENDPOINT_SYSTEM);if(!failure)++system_stops; }
uint32_t app_console_endpoint_generation(app_endpoint_t e) { return e==APP_ENDPOINT_SYSTEM ? 3 : 2; }
esp_err_t app_console_receive(app_endpoint_t e,app_message_t *m,uint32_t timeout)
{ assert(e==APP_ENDPOINT_SYSTEM && !timeout);if(!queued)return ESP_ERR_TIMEOUT;*m=inbox;queued=false;return ESP_OK; }
esp_err_t app_console_reply(const app_message_t *m,app_message_t *r)
{ assert(m->type==inbox.type);reply=*r;return ESP_OK; }
void app_message_release(app_message_t *m) { m->lease=NULL;++released; }
int64_t esp_timer_get_time(void) { return 100; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps;return 1; }
bool app_restart_pending(void) { return false; }
esp_err_t app_core_enter_normal(void) { return app_core_mode_enter_normal(&app_core_mode)?ESP_OK:ESP_ERR_INVALID_STATE; }
bool app_restart_prepare(void) { return true; }
bool app_restart_commit(unsigned delay) { (void)delay;return true; }
void app_restart_cancel(void) {}
esp_err_t app_core_camera_session(const app_message_t *m) { (void)m;return ESP_OK; }
static esp_err_t dispatch(void) { queued=true;app_core_messages_poll();return reply.result; }
int main(void)
{
    for(failure=1;failure<=5;++failure) {
        memset(cleanup,0,sizeof(cleanup));cleanup_at=0;
        assert(app_core_messages_start()==ESP_ERR_NO_MEM && !strcmp(cleanup,"IPU"));
        assert(cleanup_router==(failure<3?failure:2));
    }
    failure=0;
    assert(app_core_messages_start()==ESP_OK);
    inbox=(app_message_t){.type=APP_MESSAGE_SYSTEM_FACTORY_RESET,.source=APP_ENDPOINT_UI,
        .target=APP_ENDPOINT_SYSTEM,.flags=APP_MESSAGE_REQUEST,.generation=2,.endpoint_epoch=3,.deadline_us=1000};
    assert(dispatch()==ESP_ERR_NOT_SUPPORTED && !requested && !queried);
    inbox.type=APP_MESSAGE_SYSTEM_FACTORY_RESULT;assert(dispatch()==ESP_ERR_NOT_SUPPORTED);
    inbox.source=APP_ENDPOINT_UART;inbox.type=APP_MESSAGE_SYSTEM_FACTORY_RESET;
    assert(dispatch()==ESP_ERR_NOT_SUPPORTED && !requested && released==3);
    assert(!app_core_messages_quiesce(50) && system_stops==1 && router_stops==1);
    router_stop_ok=true;
    assert(app_core_messages_quiesce(50) && system_stops==2 && router_stops==2 && !requested && released==3);
    assert(app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP);
    inbox=(app_message_t){.type=APP_MESSAGE_SYSTEM_ENTER_NORMAL,.source=APP_ENDPOINT_UART,
        .target=APP_ENDPOINT_SYSTEM,.flags=APP_MESSAGE_REQUEST,.generation=2,.endpoint_epoch=3,.deadline_us=1000};
    assert(dispatch()==ESP_ERR_INVALID_ARG);
    inbox.source=APP_ENDPOINT_UI;inbox.payload.command.index=99;assert(dispatch()==ESP_ERR_INVALID_ARG);
    inbox.payload.command.index=APP_NORMAL_LIVE;inbox.deadline_us=100;assert(dispatch()==ESP_ERR_TIMEOUT);
    inbox.deadline_us=1000;inbox.generation=1;assert(dispatch()==ESP_ERR_INVALID_STATE);
    inbox.generation=2;assert(dispatch()==ESP_OK && app_core_mode_get(&app_core_mode)==APP_CORE_NORMAL);
    inbox.payload.command.index=APP_NORMAL_SETTINGS;assert(dispatch()==ESP_OK);
    assert(!app_core_mode_request_maintenance(&app_core_mode));
    app_core_mode_restart(&app_core_mode);assert(dispatch()==ESP_ERR_INVALID_STATE);
    puts("System rejects retired normal factory messages and preserves mode/stop contracts");
}
