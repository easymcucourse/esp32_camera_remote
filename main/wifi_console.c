#include "debug_console.h"
#include "wifi_console.h"
#include "wifi_ap.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static uint32_t pending_token, reset_deadline;
static bool reveal_password, reset_confirm;
static bool reset_all, pending_all;
static char pending_password[WIFI_PASSWORD_MAX + 1];
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
static void submit(const app_wifi_config_t *config, bool reveal)
{
    if (pending_token) { debug_printf("[dbg] ERR wifi request still pending\n"); return; }
    wifi_cfg_error_t invalid = wifi_config_check(config, wifi_ap_max_channel());
    if (invalid != WIFI_CFG_OK) { debug_printf("[dbg] ERR wifi %s\n", wifi_config_error_text(invalid)); return; }
    esp_err_t err = wifi_ap_request_apply(config, &pending_token);
    if (err != ESP_OK) { pending_token = 0; debug_printf("[dbg] ERR wifi %s\n", esp_err_to_name(err)); return; }
    reveal_password = reveal;
    snprintf(pending_password, sizeof(pending_password), "%s", reveal ? config->password : "");
    debug_printf("[dbg] OK wifi queued token=%lu\n", (unsigned long)pending_token);
}
void wifi_console_poll(void)
{
    if (!pending_token) return;
    esp_err_t result;
    esp_err_t state = wifi_ap_request_result(pending_token, &result);
    if (state == ESP_ERR_NOT_FINISHED) return;
    if (state != ESP_OK) result = state;
    if (result == ESP_OK) {
        if (reveal_password) debug_printf("[dbg] wifi new password=%s\n", pending_password);
        debug_printf("[dbg] DONE %s token=%lu%s\n", pending_all ? "factory all" : "wifi", (unsigned long)pending_token,
            pending_all ? "; rebooting, confirm pairing on camera" : "");
    } else debug_printf("[dbg] FAIL wifi token=%lu %s\n", (unsigned long)pending_token, esp_err_to_name(result));
    memset(pending_password, 0, sizeof(pending_password)); pending_token = 0; reveal_password = pending_all = false;
}
bool wifi_console_command(int argc, char **argv)
{
    if (!argc) return false;
    if (!strcmp(argv[0], "factory")) {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (argc == 2 && (!strcmp(argv[1], "wifi") || !strcmp(argv[1], "all"))) {
            if (pending_token) { reset_confirm = false; debug_printf("[dbg] ERR factory request pending\n"); return true; }
            reset_all = !strcmp(argv[1], "all");
            reset_confirm = true; reset_deadline = now + 10000;
            debug_printf("[dbg] OK factory confirm within 10 seconds to reset %s\n", reset_all ? "Wi-Fi and camera identity; device will reboot" : "Wi-Fi");
        } else if (argc == 2 && !strcmp(argv[1], "confirm")) {
            bool confirmed = reset_confirm && (int32_t)(now - reset_deadline) < 0;
            reset_confirm = false;
            if (!confirmed) debug_printf("[dbg] ERR factory nothing to confirm\n");
            else if (pending_token) debug_printf("[dbg] ERR factory request pending\n");
            else {
                esp_err_t err = wifi_ap_request_reset(reset_all, &pending_token);
                if (err != ESP_OK) { pending_token = 0; debug_printf("[dbg] ERR factory %s\n", esp_err_to_name(err)); }
                else { pending_all = reset_all; reveal_password = false; debug_printf("[dbg] OK factory queued token=%lu\n", (unsigned long)pending_token); }
            }
        } else { reset_confirm = false; debug_printf("[dbg] ERR factory use wifi / all / confirm\n"); }
        return true;
    }
    if (strcmp(argv[0], "wifi")) return false;
    app_wifi_config_t config; wifi_ap_get_config(&config);
    if (argc >= 2 && !strcmp(argv[1], "show") &&
        (argc == 2 || (argc == 3 && !strcmp(argv[2], "password")))) {
        debug_printf("[dbg] wifi SSID=%s channel=%u max_channel=%u password_len=%u display=%s default_password=%d\n", config.ssid,
            config.channel, wifi_ap_max_channel(), (unsigned)strlen(config.password), config.show_password ? "on" : "off",wifi_config_uses_default_password(&config));
        if (argc == 3) debug_printf("[dbg] wifi password=%s\n", config.password);
        wifi_ap_log_clients(); debug_printf("[dbg] OK wifi show\n");
    } else if (argc >= 4 && !strcmp(argv[1], "set") && !(argc % 2)) {
        unsigned seen = 0;
        for (int i = 2; i < argc; i += 2) {
            unsigned field;
            wifi_cfg_error_t invalid = WIFI_CFG_OK;
            if (!strcmp(argv[i], "ssid")) {
                field = 1; invalid = wifi_config_check_ssid(argv[i + 1]);
                if (invalid == WIFI_CFG_OK) snprintf(config.ssid, sizeof(config.ssid), "%s", argv[i + 1]);
            } else if (!strcmp(argv[i], "password")) {
                field = 2; invalid = wifi_config_check_password(argv[i + 1]);
                if (invalid == WIFI_CFG_OK) snprintf(config.password, sizeof(config.password), "%s", argv[i + 1]);
            } else if (!strcmp(argv[i], "channel")) {
                unsigned channel; field = 4;
                if (!number(argv[i + 1], &channel) || wifi_config_check_channel(channel, wifi_ap_max_channel()) != WIFI_CFG_OK) invalid = WIFI_CFG_CHANNEL_RANGE;
                else config.channel = channel;
            } else { debug_printf("[dbg] ERR wifi unknown key\n"); return true; }
            if (invalid != WIFI_CFG_OK) { debug_printf("[dbg] ERR wifi %s\n", wifi_config_error_text(invalid)); return true; }
            if (seen & field) { debug_printf("[dbg] ERR wifi duplicate key\n"); return true; }
            seen |= field;
        }
        submit(&config, false);
    } else if (argc == 3 && !strcmp(argv[1], "display") && (!strcmp(argv[2], "on") || !strcmp(argv[2], "off"))) {
        config.show_password = !strcmp(argv[2], "on"); submit(&config, false);
    } else if (argc == 2 && !strcmp(argv[1], "newpass")) {
        wifi_config_make_password(config.password, esp_fill_random); submit(&config, true);
    } else debug_printf("[dbg] ERR wifi use show [password], set key value..., display on|off, newpass\n");
    return true;
}
