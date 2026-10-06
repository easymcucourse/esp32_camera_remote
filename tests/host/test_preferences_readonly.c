#include "ui_preferences.h"
#include "ui_preferences_console.h"
#include "preferences_store.h"
#include "app_console.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static unsigned loads,sets,calls,releases;
static bool load_failed;
static char output[512];
esp_err_t preferences_store_load(preferences_config_t *value)
{ ++loads;if(load_failed)return ESP_FAIL;*value=(preferences_config_t){1,1,2};return ESP_OK; }
void app_ui_set_info_level(unsigned value) { assert(value==(load_failed?0:2));++sets; }
int64_t esp_timer_get_time(void) { return 1000; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_UART);return 2; }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "error"; }
int debug_printf(const char *format,...)
{ va_list args;va_start(args,format);int n=vsnprintf(output,sizeof output,format,args);va_end(args);return n; }
void app_message_release(app_message_t *message) { assert(!message->lease);++releases; }
esp_err_t app_console_request(app_message_t *message,app_message_t *reply)
{
    ++calls;bool deferred=true;assert(message->payload.command.index==APP_UI_PREF_GET);
    reply->result=ui_preferences_message(message,reply,&deferred);assert(!deferred);return ESP_OK;
}
int main(void)
{
    assert(ui_preferences_start()==ESP_OK && loads==1 && sets==1);
    assert(ui_preferences_level()==2 && ui_preferences_pad()==1);
    assert(ui_preferences_start()==ESP_OK && loads==1);
    char *info[]={"ui","info"},*pad[]={"ui","pad"};
    assert(ui_preferences_command(2,info) && strstr(output,"hidden"));
    assert(ui_preferences_command(2,pad) && strstr(output,"xbox"));
    unsigned before=calls;
    char *write[]={"ui","info","next"};assert(ui_preferences_command(3,write) && calls==before && strstr(output,"Web"));
    write[1]="pad";write[2]="ds";assert(ui_preferences_command(3,write) && calls==before);
    app_message_t message={.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_INPUT,
        .flags=APP_MESSAGE_REQUEST,.generation=2,.deadline_us=2000},reply={0};bool deferred=true;
    for(unsigned op=APP_UI_PREF_INFO_SET;op<APP_UI_PREF_COUNT;++op) {
        message.payload.command.index=op;
        assert(ui_preferences_message(&message,&reply,&deferred)==ESP_ERR_NOT_SUPPORTED && !deferred);
    }
    assert(ui_preferences_level()==2 && ui_preferences_pad()==1 && loads==1 && calls==releases);
    assert(ui_preferences_quiesce(0));message.payload.command.index=APP_UI_PREF_GET;
    assert(ui_preferences_message(&message,&reply,&deferred)==ESP_ERR_INVALID_STATE);
    load_failed=true;assert(ui_preferences_start()==ESP_OK && loads==2 && sets==2);
    assert(ui_preferences_level()==0 && ui_preferences_pad()==0);
    puts("Boot-only preference loading; all normal writes rejected without task or store writer");return 0;
}
