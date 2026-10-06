#include "debug_console.h"
#include "async_token.h"
#include "debug_line.h"
#include "debug_args.h"
#include "atom_protocol.h"
#include "driver/uart.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdatomic.h>
uint32_t debug_async_token(void)
{
    return async_token_next();
}

static debug_command_t application_command;
static void (*application_poll)(void);
static void (*application_retire)(void);
static atomic_bool started, stopping, worker_exists;
static uint32_t request_id;
static bool numbered;
int debug_printf(const char *format, ...)
{
    va_list args; va_start(args, format);
    flockfile(stdout);
    if (numbered && !strncmp(format, "[dbg] ", 6)) {
        fprintf(stdout, "[dbg] #%" PRIu32 " ", request_id);
        format += 6;
    }
    int result = vprintf(format, args);
    funlockfile(stdout);
    va_end(args); return result;
}

static bool common_command(int argc, char **argv)
{
    if (!strcmp(argv[0], "version")) {
        if (argc != 1) { debug_printf("[dbg] ERR usage: version\n"); return true; }
        const esp_app_desc_t *app = esp_app_get_description();
        debug_printf("[dbg] OK version project=%s firmware=%s date=%s time=%s idf=%s protocol=%u\n",
               app->project_name, app->version, app->date, app->time, app->idf_ver, ATOM_PROTOCOL_VERSION);
        return true;
    }
    if (strcmp(argv[0], "log")) return false;
    static const char *const levels[] = {"none", "error", "warn", "info", "debug", "verbose"};
    if (argc == 3) {
        for (unsigned i = 0; i < sizeof(levels) / sizeof(levels[0]); ++i) {
            if (!strcmp(argv[2], levels[i])) {
                esp_log_level_set(argv[1], (esp_log_level_t)i);
                debug_printf("[dbg] OK log tag=%s level=%s (limited by compiled maximum)\n", argv[1], levels[i]);
                return true;
            }
        }
    }
    debug_printf("[dbg] ERR usage: log <tag|*> <none|error|warn|info|debug|verbose>\n");
    return true;
}

static void console_task(void *unused)
{
    (void)unused;
    debug_line_t line = {0};
    while (!atomic_load(&stopping)) {
        uint8_t byte;
        if (application_poll) application_poll();
        int read=uart_read_bytes(UART_NUM_0, &byte, 1, pdMS_TO_TICKS(20));
        if (read<0) break;
        if (read!=1) continue;
        if (atomic_load(&stopping)) break;
        debug_line_result_t result = debug_line_feed(&line, byte);
        if (result == DEBUG_LINE_ERROR) { printf("[dbg] ERR input line invalid or too long\n"); continue; }
        if (result != DEBUG_LINE_READY) continue;
        char *argv[64];
        int argc = debug_split_args(line.text, argv, 64);
        if (argc < 0) { printf("[dbg] ERR input malformed quoting or too many arguments\n"); continue; }
        char **command = argv;
        if (argc && argv[0][0] == '#') {
            if (!debug_request_id(argv[0], &request_id)) { printf("[dbg] ERR invalid request number\n"); continue; }
            numbered = true; --argc; ++command;
        }
        if (!argc && numbered) debug_printf("[dbg] ERR missing command\n");
        if (argc && !common_command(argc, command) && !application_command(argc, command))
            debug_printf("[dbg] ERR unknown command; use help\n");
        numbered = false;
    }
    numbered=false;
    if (application_retire) application_retire();
    while (uart_driver_delete(UART_NUM_0)!=ESP_OK) vTaskDelay(pdMS_TO_TICKS(20));
    application_command=NULL;application_poll=NULL;application_retire=NULL;
    atomic_store(&worker_exists,false);
    atomic_store(&started,false);
    vTaskDelete(NULL);
}

esp_err_t debug_console_start_owner(debug_command_t dispatch, void (*poll)(void),void (*retire)(void))
{
    bool expected=false;
    if (!dispatch || !atomic_compare_exchange_strong(&started,&expected,true)) return ESP_ERR_INVALID_STATE;
    esp_err_t err = uart_driver_install(UART_NUM_0, 512, 0, 0, NULL, 0);
    if (err != ESP_OK) { atomic_store(&started,false);return err; }
    atomic_store(&stopping,false);
    application_command = dispatch; application_poll = poll;application_retire=retire;
    atomic_store(&worker_exists,true);
    if (xTaskCreate(console_task, "debug_console", 4096, NULL, 2, NULL) != pdPASS) {
        atomic_store(&worker_exists,false);
        if (uart_driver_delete(UART_NUM_0)==ESP_OK) {
            application_command=NULL;application_poll=NULL;application_retire=NULL;atomic_store(&started,false);
        } else {
            /* No worker exists. Retain ownership for stop() to retry cleanup. */
            atomic_store(&stopping,true);
        }
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool debug_console_stop(uint32_t timeout_ms)
{
    atomic_store(&stopping,true);
    TickType_t begin=xTaskGetTickCount(),budget=pdMS_TO_TICKS(timeout_ms);
    while (atomic_load(&started)) {
        if (!atomic_load(&worker_exists) && uart_driver_delete(UART_NUM_0)==ESP_OK) {
            application_command=NULL;application_poll=NULL;application_retire=NULL;atomic_store(&started,false);break;
        }
        if ((TickType_t)(xTaskGetTickCount()-begin)>=budget) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

esp_err_t debug_console_start(debug_command_t dispatch,void (*poll)(void))
{ return debug_console_start_owner(dispatch,poll,NULL); }
