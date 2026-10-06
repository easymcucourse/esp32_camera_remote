#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "app_console.h"
#include "network_config.h"
#include "../support/legacy/wifi_console_noreset.c"

static int64_t clock_us;
static uint32_t uart_epoch=1,wifi_epoch=2,system_epoch=3;
static network_config_t stored;
static unsigned prepares,commits,cancels,factories,queries;
static bool commit_timeout,result_timeout,result_busy;
static esp_err_t completed;
static char output[8192];
static int64_t prepare_deadline;
int64_t esp_timer_get_time(void) { return clock_us; }
const char *esp_err_to_name(esp_err_t err) { return err==ESP_OK ? "OK" : "ERROR"; }
void esp_fill_random(void *data,size_t size) { memset(data,1,size); }
int debug_printf(const char *format,...)
{
    va_list args;va_start(args,format);
    int n=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return n;
}
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ return endpoint==APP_ENDPOINT_UART ? uart_epoch : endpoint==APP_ENDPOINT_WIFI ? wifi_epoch : system_epoch; }
void app_message_release(app_message_t *message) { memset(message,0,sizeof(*message)); }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->type==APP_MESSAGE_WIFI_CONFIG_CANCEL && message->payload.command.token==7);
    assert(!message->flags && message->generation==uart_epoch);++cancels;return ESP_OK;
}
esp_err_t app_console_request(app_message_t *request,app_message_t *reply)
{
    assert(request->source==APP_ENDPOINT_UART && request->generation==uart_epoch);
    assert(request->flags==APP_MESSAGE_REQUEST && request->deadline_us>clock_us && !request->lease);
    *reply=(app_message_t){.result=ESP_OK};
    switch (request->type) {
    case APP_MESSAGE_WIFI_STATUS:
        memcpy(reply->payload.network.config.ssid,stored.ssid,sizeof(stored.ssid));
        memcpy(reply->payload.network.config.password,stored.password,sizeof(stored.password));
        reply->payload.network.config.channel=stored.channel;
        reply->payload.network.config.show_password=stored.show_password;
        reply->payload.network.max_channel=11;break;
    case APP_MESSAGE_CAMERA_DISCOVER: reply->payload.discovery.count=2;break;
    case APP_MESSAGE_WIFI_CONFIG_PREPARE:
        ++prepares;prepare_deadline=request->deadline_us;
        assert(request->target==APP_ENDPOINT_WIFI);
        stored.channel=request->payload.config.channel;
        stored.show_password=request->payload.config.show_password;
        memcpy(stored.password,request->payload.config.password,sizeof(stored.password));
        reply->payload.command.token=7;break;
    case APP_MESSAGE_WIFI_CONFIG_COMMIT:
        ++commits;assert(request->deadline_us==prepare_deadline);
        assert(request->payload.command.token==7 && request->payload.command.duration_ms==0);
        if (commit_timeout) return ESP_ERR_TIMEOUT;
        break;
    case APP_MESSAGE_WIFI_CONFIG_RESULT:
        ++queries;
        assert(request->target==APP_ENDPOINT_WIFI);
        if (result_timeout) return ESP_ERR_TIMEOUT;
        reply->result=result_busy ? ESP_ERR_NOT_FINISHED : ESP_OK;
        reply->payload.command.value=completed;break;
    default: assert(false);
    }
    return ESP_OK;
}
static void command(int argc,char **argv)
{ output[0]=0;assert(wifi_console_command(argc,argv)); }
int main(void)
{
    network_config_make_default(&stored);
    char *bad[]={"wifi","set","channel","12"};command(4,bad);
    assert(strstr(output,"ERR wifi") && !prepares);
    char *display[]={"wifi","display","on"};command(3,display);
    assert(prepares==1 && commits==1 && pending_token==7 && stored.show_password);
    result_busy=true;wifi_console_poll();assert(pending_token==7);
    result_timeout=true;wifi_console_poll();assert(pending_token==7);
    result_timeout=result_busy=false;wifi_console_poll();assert(!pending_token && strstr(output,"DONE wifi"));
    commit_timeout=true;
    char *newpass[]={"wifi","newpass"};command(2,newpass);
    assert(pending_token==7 && cancels==1 && !strstr(output,"new password="));
    wifi_console_poll();assert(!pending_token && strstr(output,"new password=") && !pending_password[0]);
    command(2,newpass);completed=ESP_FAIL;wifi_console_poll();
    assert(strstr(output,"FAIL wifi") && !strstr(output,"new password=") && !pending_password[0]);
    completed=ESP_OK;command(2,newpass);++wifi_epoch;unsigned before=queries;wifi_console_poll();
    assert(!pending_token && queries==before && strstr(output,"FAIL wifi") && !pending_password[0]);
    char *all[]={"factory","all"},*confirm[]={"factory","confirm"};
    output[0]=0;assert(!wifi_console_command(2,all));assert(!wifi_console_command(2,confirm));
    assert(!factories && !pending_token && !output[0]);
    char *show[]={"wifi","show"};command(2,show);
    assert(strstr(output,"max_channel=11") && strstr(output,"Associated DHCP clients: 2"));
    assert(!strstr(output,"wifi password="));
    puts("UART Wi-Fi transactions passed");return 0;
}
