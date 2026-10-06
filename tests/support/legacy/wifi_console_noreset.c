#include "debug_console.h"
#include "uart_wifi_console.h"
#include "app_console.h"
#include "network_config.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static uint32_t pending_token;
static bool reveal_password;
static char pending_password[NETWORK_PASSWORD_MAX + 1];
static uint32_t pending_generation, pending_epoch;
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
static void clear_pending(void)
{
    memset(pending_password,0,sizeof(pending_password));
    pending_token=pending_generation=pending_epoch=0;
    reveal_password=false;
}
static esp_err_t queue_config(const network_config_t *config)
{
    app_endpoint_t target=APP_ENDPOINT_WIFI;
    pending_generation=app_console_endpoint_generation(APP_ENDPOINT_UART);
    pending_epoch=app_console_endpoint_generation(target);
    int64_t deadline=esp_timer_get_time()+500000;
    app_message_payload_t payload={.config={.channel=0}};
    {
        memcpy(payload.config.ssid,config->ssid,sizeof(config->ssid));
        memcpy(payload.config.password,config->password,sizeof(config->password));
        payload.config.channel=config->channel;payload.config.show_password=config->show_password;
    }
    app_message_t reply={0};
    esp_err_t err=rpc(target,APP_MESSAGE_WIFI_CONFIG_PREPARE,
        &payload,&reply,deadline);
    if (err==ESP_OK) {
        err=reply.result;
        if (err==ESP_OK) {
            pending_token=reply.payload.command.token;
            if (!pending_token) err=ESP_ERR_INVALID_RESPONSE;
        }
    }
    app_message_release(&reply);
    if (err!=ESP_OK) return err;
    payload=(app_message_payload_t){.command={.token=pending_token}};
    err=rpc(target,APP_MESSAGE_WIFI_CONFIG_COMMIT,&payload,&reply,deadline);
    if (err==ESP_OK) err=reply.result;
    app_message_release(&reply);
    if (err!=ESP_OK) {
        /* COMMIT may already have executed: keep the token until RESULT.
         * Cancellation can retire only the still-staged transaction. */
        app_message_t cancel={.type=APP_MESSAGE_WIFI_CONFIG_CANCEL,.source=APP_ENDPOINT_UART,
            .target=target,.generation=pending_generation,.deadline_us=esp_timer_get_time()+500000,
            .payload.command={.token=pending_token}};
        app_console_send(&cancel);
    }
    return err;
}
static bool number(const char *text, unsigned *value)
{
    if (!text || !*text) return false;
    unsigned parsed = 0;
    for (; *text; ++text) {
        if (*text < '0' || *text > '9' || parsed > (UINT_MAX - (unsigned)(*text - '0')) / 10) return false;
        parsed = parsed * 10 + (unsigned)(*text - '0');
    }
    *value = parsed; return true;
}
static void submit(const network_config_t *config, unsigned limit, bool reveal)
{
    if (pending_token) { debug_printf("[dbg] ERR wifi request still pending\n"); return; }
    network_cfg_error_t invalid = network_config_check(config, limit);
    if (invalid != NETWORK_CFG_OK) { debug_printf("[dbg] ERR wifi %s\n", network_config_error_text(invalid)); return; }
    esp_err_t err = queue_config(config);
    if (err != ESP_OK && !pending_token) { clear_pending(); debug_printf("[dbg] ERR wifi %s\n", esp_err_to_name(err)); return; }
    reveal_password = reveal;
    snprintf(pending_password, sizeof(pending_password), "%s", reveal ? config->password : "");
    debug_printf("[dbg] OK wifi queued token=%lu\n", (unsigned long)pending_token);
}
void wifi_console_poll(void)
{
    if (!pending_token) return;
    app_endpoint_t target=APP_ENDPOINT_WIFI;
    esp_err_t result=ESP_ERR_INVALID_STATE;
    esp_err_t state=ESP_ERR_INVALID_STATE;
    if (pending_generation==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
        pending_epoch==app_console_endpoint_generation(target)) {
        app_message_payload_t payload={.command={.token=pending_token}};
        app_message_t reply={0};
        state=rpc(target,APP_MESSAGE_WIFI_CONFIG_RESULT,
            &payload,&reply,esp_timer_get_time()+500000);
        if (state!=ESP_OK) { app_message_release(&reply);return; }
        state=reply.result;result=(esp_err_t)reply.payload.command.value;
        app_message_release(&reply);
    }
    if (state == ESP_ERR_NOT_FINISHED) return;
    if (state != ESP_OK) result = state;
    if (result == ESP_OK) {
        if (reveal_password) debug_printf("[dbg] wifi new password=%s\n", pending_password);
        debug_printf("[dbg] DONE wifi token=%lu\n", (unsigned long)pending_token);
    } else debug_printf("[dbg] FAIL wifi token=%lu %s\n", (unsigned long)pending_token, esp_err_to_name(result));
    clear_pending();
}
bool wifi_console_command(int argc, char **argv)
{
    if (!argc) return false;
    if (strcmp(argv[0], "wifi")) return false;
    network_config_t config;unsigned limit;
    esp_err_t err=snapshot(&config,&limit);
    if (err!=ESP_OK) { debug_printf("[dbg] ERR wifi %s\n",esp_err_to_name(err));return true; }
    if (argc >= 2 && !strcmp(argv[1], "show") &&
        (argc == 2 || (argc == 3 && !strcmp(argv[2], "password")))) {
        debug_printf("[dbg] wifi SSID=%s channel=%u max_channel=%u password_len=%u display=%s default_password=%d\n", config.ssid,
            config.channel, limit, (unsigned)strlen(config.password), config.show_password ? "on" : "off",network_config_uses_default_password(&config));
        if (argc == 3) debug_printf("[dbg] wifi password=%s\n", config.password);
        app_message_t reply={0};
        if (rpc(APP_ENDPOINT_WIFI,APP_MESSAGE_CAMERA_DISCOVER,NULL,&reply,esp_timer_get_time()+500000)==ESP_OK &&
            reply.result==ESP_OK && reply.payload.discovery.count<=4)
            debug_printf("[dbg] Associated DHCP clients: %u\n",(unsigned)reply.payload.discovery.count);
        app_message_release(&reply);debug_printf("[dbg] OK wifi show\n");
    } else if (argc >= 4 && !strcmp(argv[1], "set") && !(argc % 2)) {
        unsigned seen = 0;
        for (int i = 2; i < argc; i += 2) {
            unsigned field;
            network_cfg_error_t invalid = NETWORK_CFG_OK;
            if (!strcmp(argv[i], "ssid")) {
                field = 1; invalid = network_config_check_ssid(argv[i + 1]);
                if (invalid == NETWORK_CFG_OK) snprintf(config.ssid, sizeof(config.ssid), "%s", argv[i + 1]);
            } else if (!strcmp(argv[i], "password")) {
                field = 2; invalid = network_config_check_password(argv[i + 1]);
                if (invalid == NETWORK_CFG_OK) snprintf(config.password, sizeof(config.password), "%s", argv[i + 1]);
            } else if (!strcmp(argv[i], "channel")) {
                unsigned channel; field = 4;
                if (!number(argv[i + 1], &channel) || network_config_check_channel(channel, limit) != NETWORK_CFG_OK) invalid = NETWORK_CFG_CHANNEL_RANGE;
                else config.channel = channel;
            } else { debug_printf("[dbg] ERR wifi unknown key\n"); return true; }
            if (invalid != NETWORK_CFG_OK) { debug_printf("[dbg] ERR wifi %s\n", network_config_error_text(invalid)); return true; }
            if (seen & field) { debug_printf("[dbg] ERR wifi duplicate key\n"); return true; }
            seen |= field;
        }
        submit(&config, limit, false);
    } else if (argc == 3 && !strcmp(argv[1], "display") && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) {
        config.show_password = !strcmp(argv[2], "on"); submit(&config, limit, false);
    } else if (argc == 2 && !strcmp(argv[1], "newpass")) {
        network_config_make_password(config.password, esp_fill_random); submit(&config, limit, true);
    } else debug_printf("[dbg] ERR wifi use show [password], set key value..., display on|off, newpass\n");
    return true;
}
