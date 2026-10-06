#include "app_core.h"
#include "app_core_services.h"
#include "app_core_mode.h"
#include "app_core_maintenance.h"
#include "app_core_messages.h"
#include "app_core_camera.h"
#include "app_ui.h"
#include "app_input.h"
#include "app_restart.h"
#include "app_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
static void boot_message_stage(const char *stage, esp_err_t error)
{
    ESP_LOGI("core", "Messages %s: %s; internal=%u largest=%u psram=%u envelope=%u", stage,
        esp_err_to_name(error), (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), (unsigned)sizeof(app_message_t));
}
#else
#define boot_message_stage(stage, error) ((void)0)
#endif

esp_err_t app_core_messages_start(void)
{
    esp_err_t error = app_console_router_start();
    boot_message_stage("router", error);
    if (error != ESP_OK) return error;
    const app_endpoint_config_t system = {8, 1};
    error = app_console_endpoint_register(APP_ENDPOINT_SYSTEM, &system);
    boot_message_stage("system", error);
    if (error == ESP_OK) { error = app_ui_messages_start(); boot_message_stage("UI", error); }
    if (error == ESP_OK) { error = app_input_start(); boot_message_stage("input", error); }
    if (error != ESP_OK) {
        /* A started UI owner must exit before router teardown, even when the
         * following Input allocation failed. Preserve routing on drain failure. */
        bool input=app_input_quiesce(1000)==ESP_OK;
        bool preferences=app_ui_preferences_quiesce(1000);
        bool ui=app_ui_messages_quiesce(1000);
        if (input && preferences && ui) {
            app_console_endpoint_stop(APP_ENDPOINT_SYSTEM);
            (void)app_console_router_quiesce(1000);
        }
    }
    return error;
}

void app_core_messages_poll(void)
{
    app_message_t request, reply;
    while (app_console_receive(APP_ENDPOINT_SYSTEM, &request, 0) == ESP_OK) {
        reply = (app_message_t){0};
        app_core_mode_value_t current=app_core_mode_get(&app_core_mode);
        if (current!=APP_CORE_STARTUP && current!=APP_CORE_NORMAL &&
            request.type!=APP_MESSAGE_SYSTEM_STATUS && request.type!=APP_MESSAGE_SYSTEM_CAMERA_SESSION)
            reply.result=ESP_ERR_INVALID_STATE;
        else if (request.type == APP_MESSAGE_SYSTEM_STATUS) {
            app_core_mode_value_t mode=app_core_mode_get(&app_core_mode);
            reply.payload.system.mode=mode==APP_CORE_STARTUP ? APP_SYSTEM_MODE_STARTUP : mode==APP_CORE_NORMAL ? APP_SYSTEM_MODE_NORMAL :
                mode==APP_CORE_ACTIVATING ? APP_SYSTEM_MODE_ACTIVATING : mode==APP_CORE_MAINTENANCE ? APP_SYSTEM_MODE_MAINT : APP_SYSTEM_MODE_RESTART;
            reply.payload.system.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
            reply.payload.system.free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            reply.payload.system.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            reply.payload.system.restart_pending = app_restart_pending();
            reply.result = ESP_OK;
        } else if(request.type==APP_MESSAGE_SYSTEM_ENTER_NORMAL) {
            unsigned limit=APP_NORMAL_SETTINGS;
#if CONFIG_REMOTE_DBG_SIM
            limit=APP_NORMAL_DISPLAY_TEST;
#endif
            if(request.source!=APP_ENDPOINT_UI || request.flags!=APP_MESSAGE_REQUEST || request.lease ||
                !request.generation || request.payload.command.index>limit)reply.result=ESP_ERR_INVALID_ARG;
            else if(request.deadline_us<=esp_timer_get_time())reply.result=ESP_ERR_TIMEOUT;
            else if(request.generation!=app_console_endpoint_generation(APP_ENDPOINT_UI) ||
                request.endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_SYSTEM))reply.result=ESP_ERR_INVALID_STATE;
            else reply.result=app_core_enter_normal();
        } else if (request.type == APP_MESSAGE_SYSTEM_RESTART) {
            unsigned delay = request.payload.command.duration_ms;
            if (delay < 500) delay = 500;
            if (delay > 10000) reply.result = ESP_ERR_INVALID_ARG;
            else if (!app_restart_prepare()) reply.result = ESP_ERR_INVALID_STATE;
            else if (!app_restart_commit(delay)) { app_restart_cancel(); reply.result = ESP_ERR_INVALID_STATE; }
            else reply.result = ESP_OK;
        } else if (request.type==APP_MESSAGE_SYSTEM_CAMERA_SESSION) reply.result=app_core_camera_session(&request);
        else reply.result = ESP_ERR_NOT_SUPPORTED;
        if (request.flags & APP_MESSAGE_REQUEST) app_console_reply(&request, &reply);
        app_message_release(&request);
    }
}
bool app_core_messages_quiesce(uint32_t timeout_ms)
{
    app_console_endpoint_stop(APP_ENDPOINT_SYSTEM);
    return app_console_router_quiesce(timeout_ms);
}
