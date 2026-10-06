#include "app_console.h"
#include "sdkconfig.h"
#include "uart_camera_commands.h"
#include "uart_status.h"
#include "app_console.h"
#include "uart_wifi_console.h"
#include "debug_console.h"
#include "i2c_console.h"
#include "lcd_sim.h"
#include "ui_preferences_console.h"
#include "display_bench.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static bool uart_started,cleanup_pending,prefs_subscribed;
#if CONFIG_REMOTE_DBG_SIM
static bool bench_subscribed;
#endif
static bool command(int argc, char **argv)
{
#if CONFIG_REMOTE_DBG_SIM
    if (display_bench_command(argc,argv)) return true;
#endif
    if (i2c_console_command(argc,argv)) return true;
    if (ui_preferences_command(argc,argv)) return true;
#if CONFIG_REMOTE_DBG_SIM
    if (lcd_sim_command(argc,argv)) return true;
#endif
    if (wifi_console_command(argc, argv)) return true;
    if (camera_commands_command(argc,argv)) return true;
    if (argc == 1 && !strcmp(argv[0], "help")) {
        debug_printf("[dbg] OK help: version; status; extra status; log <tag|*> <level>; i2c log on|off|changes; i2c stats [reset]; j / s / S / p; wifi show [password]\n");
        debug_printf("[dbg] ui info; ui pad: query boot settings; changes are made in startup maintenance Web\n");
#if CONFIG_REMOTE_DBG_SIM
        debug_printf("[dbg] display fault off|once|persistent (RAM only)\n");
        debug_printf("[dbg] display bench: synthetic JPEG decode/overlay/publish profile; pauses camera and restores it\n");
        debug_printf("[dbg] SIM help: atom sim on|off; atom online|offline|reboot; atom version; atom fail|crc|timeout; pad connect|disconnect|gap|overflow; pad battery; gimbal state; tap; hold; release; stick; trigger; shoot; record; seq\n");
#endif
        return true;
    }
    if (uart_status_command(argc,argv)) return true;
    return false;
}

static void poll(void)
{
    app_message_t event;
    while (app_console_receive(APP_ENDPOINT_UART,&event,0)==ESP_OK) {
#if CONFIG_REMOTE_DBG_SIM
        lcd_sim_event(&event);display_bench_event(&event);
#endif
        app_message_release(&event);
    }
    i2c_console_poll();
#if CONFIG_REMOTE_DBG_SIM
    display_bench_poll();
#endif
}
static void retire_uart(void)
{ app_console_endpoint_stop(APP_ENDPOINT_UART); }
esp_err_t app_console_uart_start(void)
{
    if (uart_started) return ESP_ERR_INVALID_STATE;
    if (cleanup_pending) {
        if (!debug_console_stop(0)) return ESP_ERR_INVALID_STATE;
        cleanup_pending=false;
    }
    app_console_status_t router;
    app_console_get_status(&router);
    if (!router.running || !router.accepting) return ESP_ERR_INVALID_STATE;
    /* Router restart clears its fixed subscriptions. Compose UART before
     * freezing; a UART-only restart reuses the already frozen subscription. */
    if (!router.subscriptions_frozen) {
        prefs_subscribed=false;
#if CONFIG_REMOTE_DBG_SIM
        bench_subscribed=false;
#endif
    }
    const app_endpoint_config_t endpoint={8,1};
    esp_err_t err=app_console_endpoint_register(APP_ENDPOINT_UART,&endpoint);
    if (err!=ESP_OK) return err;
    if (!prefs_subscribed) {
        err=app_console_subscribe(APP_MESSAGE_UI_PREFERENCES,APP_ENDPOINT_UART);
        prefs_subscribed=err==ESP_OK;
    }
#if CONFIG_REMOTE_DBG_SIM
    if (err==ESP_OK && !bench_subscribed) {
        err=app_console_subscribe(APP_MESSAGE_DISPLAY_BENCH,APP_ENDPOINT_UART);
        bench_subscribed=err==ESP_OK;
    }
#endif
    if (err==ESP_OK) err=debug_console_start_owner(command,poll,retire_uart);
    if (err!=ESP_OK) {
        app_console_endpoint_stop(APP_ENDPOINT_UART);
        cleanup_pending=!debug_console_stop(0);return err;
    }
    uart_started=true;
    ESP_LOGI("camera_pair", "UART line console: help, version, status, log, display, j, S, s, p, wifi; press Enter");
    return ESP_OK;
}
bool app_console_uart_quiesce(uint32_t timeout_ms)
{
    app_console_endpoint_stop(APP_ENDPOINT_UART);
    bool stopped=debug_console_stop(timeout_ms);
    if (stopped) uart_started=cleanup_pending=false;
    return stopped;
}
