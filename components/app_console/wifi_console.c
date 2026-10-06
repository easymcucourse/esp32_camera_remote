#include "debug_console.h"
#include "uart_wifi_console.h"
#include "app_console.h"
#include "network_config.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

static esp_err_t rpc(app_endpoint_t target, app_message_type_t type,
    const app_message_payload_t *payload, app_message_t *reply, int64_t deadline)
{
    app_message_t request = {.type=type, .source=APP_ENDPOINT_UART, .target=target,
        .flags=APP_MESSAGE_REQUEST, .generation=app_console_endpoint_generation(APP_ENDPOINT_UART),
        .deadline_us=deadline};
    if (payload) request.payload=*payload;
    return app_console_request(&request,reply);
}
static esp_err_t snapshot(network_config_t *config, unsigned *limit)
{
    app_message_t reply={0};
    esp_err_t err=rpc(APP_ENDPOINT_WIFI,APP_MESSAGE_WIFI_STATUS,NULL,&reply,esp_timer_get_time()+500000);
    if (err==ESP_OK) {
        err=reply.result;
        if (err==ESP_OK) {
            const app_network_config_t *value=&reply.payload.network.config;
            *config=(network_config_t){.channel=value->channel,.show_password=value->show_password};
            memcpy(config->ssid,value->ssid,sizeof(config->ssid));
            memcpy(config->password,value->password,sizeof(config->password));
            *limit=reply.payload.network.max_channel;
            if (!memchr(config->ssid,0,sizeof(config->ssid)) ||
                !memchr(value->password,0,sizeof(config->password)) ||
                *limit<1 || *limit>13 || network_config_check(config,*limit)!=NETWORK_CFG_OK)
                err=ESP_ERR_INVALID_RESPONSE;
        }
    }
    app_message_release(&reply);return err;
}
bool wifi_console_command(int argc,char **argv)
{
    if (!argc || strcmp(argv[0],"wifi")) return false;
    if (argc<2 || strcmp(argv[1],"show") ||
        (argc!=2 && !(argc==3 && !strcmp(argv[2],"password")))) {
        debug_printf("[dbg] ERR wifi settings are changed in maintenance Web; use wifi show [password]\n");return true;
    }
    network_config_t config;unsigned limit;
    esp_err_t error=snapshot(&config,&limit);
    if (error!=ESP_OK) { debug_printf("[dbg] ERR wifi %s\n",esp_err_to_name(error));return true; }
    debug_printf("[dbg] wifi SSID=%s channel=%u max_channel=%u password_len=%u display=%s default_password=%d\n",
        config.ssid,config.channel,limit,(unsigned)strlen(config.password),
        config.show_password ? "on" : "off",network_config_uses_default_password(&config));
    if (argc==3) debug_printf("[dbg] wifi password=%s\n",config.password);
    app_message_t reply={0};
    if (rpc(APP_ENDPOINT_WIFI,APP_MESSAGE_CAMERA_DISCOVER,NULL,&reply,esp_timer_get_time()+500000)==ESP_OK &&
        reply.result==ESP_OK && reply.payload.discovery.count<=4)
        debug_printf("[dbg] Associated DHCP clients: %u\n",reply.payload.discovery.count);
    app_message_release(&reply);memset(config.password,0,sizeof config.password);
    debug_printf("[dbg] OK wifi show\n");return true;
}
