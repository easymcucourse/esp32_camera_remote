#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../support/legacy/ui_preferences_console.c"
static uint32_t uart_epoch=4,ui_epoch=5,next_token=10;
static unsigned calls,info=1,pad=1;
static esp_err_t error;
static char output[4096];
int64_t esp_timer_get_time(void) { return 1000; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ assert(endpoint==APP_ENDPOINT_UI || endpoint==APP_ENDPOINT_UART);return endpoint==APP_ENDPOINT_UI ? ui_epoch : uart_epoch; }
const char *esp_err_to_name(esp_err_t value) { return value==ESP_OK ? "ESP_OK" : "ERROR"; }
int debug_printf(const char *format,...)
{
    va_list args;va_start(args,format);
    int n=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return n;
}
void app_message_release(app_message_t *message) { memset(message,0,sizeof(*message)); }
esp_err_t app_console_request(app_message_t *message,app_message_t *reply)
{
    ++calls;assert(message->type==APP_MESSAGE_UI_PREFERENCES && message->source==APP_ENDPOINT_UART &&
        message->target==APP_ENDPOINT_UI && message->flags==APP_MESSAGE_REQUEST &&
        message->generation==uart_epoch && message->deadline_us==501000 && !message->lease);
    *reply=(app_message_t){.result=error};if (error) return ESP_OK;
    unsigned op=message->payload.command.index;
    if (op==APP_UI_PREF_GET) { assert(!message->payload.command.flag);reply->payload.command.value=info;reply->payload.command.direction=pad; }
    else if (op==APP_UI_PREF_PAD_SET) { assert(!message->payload.command.flag);pad=message->payload.command.value;reply->payload.command.direction=pad; }
    else { assert(message->payload.command.flag && (op==APP_UI_PREF_INFO_NEXT || op==APP_UI_PREF_INFO_SET));reply->payload.command.token=++next_token; }
    return ESP_OK;
}
static void command(int argc,char **argv) { output[0]=0;assert(ui_preferences_command(argc,argv)); }
static app_message_t event(uint32_t token)
{
    return (app_message_t){.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_UI,.flags=APP_MESSAGE_EVENT,
        .generation=ui_epoch,.endpoint_epoch=uart_epoch,
        .payload.command={.token=token,.value=2,.index=APP_UI_PREF_INFO_NEXT},.result=ESP_OK};
}
int main(void)
{
    assert(!ui_preferences_command(0,NULL));
    char *get[]={"ui","info"};command(2,get);assert(strstr(output,"info=compact"));
    char *pad_get[]={"ui","pad"};command(2,pad_get);assert(strstr(output,"pad=xbox"));
    char *pad_set[]={"ui","pad","ds"};command(3,pad_set);assert(strstr(output,"pad=ds result=ESP_OK"));
    char *next[]={"ui","info","next"};command(3,next);assert(strstr(output,"queued token=11"));
    app_message_t m=event(11);m.type=APP_MESSAGE_INPUT_SIM_COMMAND;ui_preferences_event(&m);
    assert(!strstr(output,"DONE"));
    m=event(11);--m.generation;ui_preferences_event(&m);assert(!strstr(output,"DONE"));
    m=event(11);ui_preferences_event(&m);assert(strstr(output,"DONE ui token=11 info=hidden"));
    output[0]=0;ui_preferences_event(&m);assert(!output[0]);
    command(3,next);m=event(12);m.result=ESP_FAIL;ui_preferences_event(&m);assert(strstr(output,"FAIL ui"));
    command(3,next);m=event(13);++uart_epoch;output[0]=0;ui_preferences_event(&m);assert(!output[0]);
    for (unsigned i=0;i<8;++i) command(3,next);
    unsigned prior=calls;command(3,next);assert(calls==prior && strstr(output,"requests pending"));
    ++ui_epoch;error=ESP_FAIL;command(3,next);assert(strstr(output,"ERR ui") && calls==prior+1);
    error=ESP_OK;command(3,next);assert(strstr(output,"queued"));
    char *bad[]={"ui","info","bogus"};prior=calls;command(3,bad);assert(calls==prior);
    puts("UART preference requests and completion lifetimes passed");return 0;
}
