#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_console/camera_commands.c"
static app_message_t sent[16];
static unsigned count,releases;
static esp_err_t ui_error,camera_error,transport_error;
static char output[2048];
int64_t esp_timer_get_time(void) { return 1000; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_UART);return 7; }
const char *esp_err_to_name(esp_err_t value) { return value==ESP_OK ? "ESP_OK" : "ERROR"; }
int debug_printf(const char *format,...)
{
    va_list args;va_start(args,format);
    int n=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return n;
}
void app_message_release(app_message_t *message) { ++releases;assert(!message->lease); }
esp_err_t app_console_request(app_message_t *message,app_message_t *reply)
{
    assert(count<16 && message->source==APP_ENDPOINT_UART && message->flags==APP_MESSAGE_REQUEST &&
        message->generation==7 && message->deadline_us==501000 && !message->lease);
    sent[count++]=*message;
    reply->result=message->target==APP_ENDPOINT_UI ? ui_error : camera_error;
    return transport_error;
}
static void command(const char *key)
{ output[0]=0;count=releases=0;char *argv[]={(char *)key};assert(camera_commands_command(1,argv));assert(releases==count); }
int main(void)
{
    assert(!camera_commands_command(0,NULL));
    char *unknown[]={"status"};assert(!camera_commands_command(1,unknown));
    command("p");assert(sent[0].type==APP_MESSAGE_CAMERA_START && sent[0].payload.command.flag && strstr(output,"OK p"));
    command("P");assert(sent[0].payload.command.flag);
    command("j");assert(sent[0].type==APP_MESSAGE_CAMERA_START && !sent[0].payload.command.flag);
    command("J");assert(!sent[0].payload.command.flag);
    command("s");assert(sent[0].type==APP_MESSAGE_CAMERA_STOP && sent[0].payload.command.flag);
    count=0;char *removed[]={"u"};assert(!camera_commands_command(1,removed) && !count);
    command("S");assert(count==2 && sent[0].target==APP_ENDPOINT_UI && sent[0].type==APP_MESSAGE_UI_MENU_ACTION &&
        sent[0].payload.action.type==PAD_ACTION_UI_TOGGLE && sent[1].target==APP_ENDPOINT_CAMERA &&
        sent[1].payload.action.type==PAD_ACTION_MF_CANCEL);
    ui_error=ESP_ERR_TIMEOUT;command("S");assert(count==2 && strstr(output,"ERR S"));ui_error=ESP_OK;
    camera_error=ESP_ERR_INVALID_STATE;command("j");assert(strstr(output,"ERR j") && !strstr(output,"OK"));camera_error=ESP_OK;
    transport_error=ESP_ERR_TIMEOUT;command("j");assert(strstr(output,"ERR j"));transport_error=ESP_OK;
    count=releases=0;output[0]=0;
    char *fault[]={"display","fault","persistent"};
#if CONFIG_REMOTE_DBG_SIM
    assert(camera_commands_command(3,fault));
    assert(count==1 && releases==1 && sent[0].type==APP_MESSAGE_DISPLAY_FAULT && sent[0].target==APP_ENDPOINT_UI && sent[0].payload.command.value==2);
    fault[2]="invalid";count=0;assert(camera_commands_command(3,fault) && !count);
#else
    assert(!camera_commands_command(3,fault) && !count);
#endif
    puts("UART camera messages, admission stop and MF cancellation passed");return 0;
}
