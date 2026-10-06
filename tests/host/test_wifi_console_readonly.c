#include "uart_wifi_console.h"
#include "app_console.h"
#include "network_config.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static unsigned calls,releases;
static char output[2048];
int64_t esp_timer_get_time(void) { return 1000; }
uint32_t app_console_endpoint_generation(app_endpoint_t e) { assert(e==APP_ENDPOINT_UART);return 2; }
const char *esp_err_to_name(esp_err_t e) { (void)e;return "error"; }
int debug_printf(const char *format,...)
{ va_list args;va_start(args,format);int n=vsnprintf(output+strlen(output),sizeof output-strlen(output),format,args);va_end(args);return n; }
void app_message_release(app_message_t *m) { assert(!m->lease);++releases; }
esp_err_t app_console_request(app_message_t *m,app_message_t *reply)
{
    assert(m->target==APP_ENDPOINT_WIFI && m->source==APP_ENDPOINT_UART && m->generation==2);++calls;
    *reply=(app_message_t){.result=ESP_OK};
    if (m->type==APP_MESSAGE_WIFI_STATUS) {
        network_config_t c;network_config_make_default(&c);
        strcpy(reply->payload.network.config.ssid,c.ssid);strcpy(reply->payload.network.config.password,c.password);
        reply->payload.network.config.channel=c.channel;reply->payload.network.config.show_password=c.show_password;
        reply->payload.network.max_channel=11;
    } else { assert(m->type==APP_MESSAGE_CAMERA_DISCOVER);reply->payload.discovery.count=1; }
    return ESP_OK;
}
int main(void)
{
    assert(!wifi_console_command(0,NULL));
    char *show[]={"wifi","show"};assert(wifi_console_command(2,show) && calls==2 && !strstr(output,"wifi password="));
    char *password[]={"wifi","show","password"};output[0]=0;
    assert(wifi_console_command(3,password) && calls==4 && strstr(output,"wifi password="));
    const char *writes[]={"set","display","newpass"};
    for(unsigned i=0;i<3;++i) { char *args[]={"wifi",(char *)writes[i],"on","6"};output[0]=0;assert(wifi_console_command(4,args) && calls==4 && strstr(output,"Web")); }
    char *factory[]={"factory","wifi"};assert(!wifi_console_command(2,factory) && calls==4);
    assert(calls==releases);puts("Normal Wi-Fi UART only queries; retired writes never reach the bus");return 0;
}
