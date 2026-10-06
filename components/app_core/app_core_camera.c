#include "app_core.h"
#include "app_core_services.h"
#include "app_core_camera.h"
#include "app_core_mode.h"
#include "app_camera.h"
#include "app_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static bool initialized,messages_started,console_started;
esp_err_t app_core_camera_boot(void)
{
    app_core_mode_value_t mode=app_core_mode_get(&app_core_mode);
    if (mode==APP_CORE_ACTIVATING && app_core_mode_booting(&app_core_mode)) return ESP_OK;
    if (mode!=APP_CORE_STARTUP && mode!=APP_CORE_NORMAL) return ESP_ERR_INVALID_STATE;
    if (app_camera_api_version()!=APP_CAMERA_API_VERSION) return ESP_ERR_NOT_SUPPORTED;
    esp_err_t error=ESP_OK;
    if (!initialized) {
        const app_camera_config_t config={.backend=APP_CAMERA_BACKEND_DEFAULT};
        error=app_camera_init(&config); if (error!=ESP_OK) return error; initialized=true;
    }
    if (!messages_started) {
        error=app_camera_messages_start(); if (error!=ESP_OK) return error; messages_started=true;
    }
    if (!console_started) {
        error=app_console_uart_start();
        if (error==ESP_OK) console_started=true;
        else { ESP_LOGE("core", "UART initialization failed: %s",esp_err_to_name(error));return error; }
    }
    return app_camera_start(APP_CAMERA_START_PREVIEW);
}
bool app_core_camera_quiesce(uint32_t timeout) { return app_camera_stop(timeout); }
bool app_core_console_ready(void) { return console_started; }
bool app_core_camera_messages_quiesce(uint32_t timeout)
{
    if (!messages_started) return true;
    if (!app_camera_messages_quiesce(timeout)) return false;
    messages_started=false;return true;
}
esp_err_t app_core_camera_session(const app_message_t *request)
{
    if (!request || request->source!=APP_ENDPOINT_CAMERA || request->lease ||
        !request->generation || (request->payload.command.flag ? request->flags!=APP_MESSAGE_REQUEST :
            (request->flags!=0 && request->flags!=APP_MESSAGE_REQUEST))) return ESP_ERR_INVALID_ARG;
    if (!messages_started) return ESP_ERR_INVALID_STATE;
    if (request->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    app_core_mode_value_t mode=app_core_mode_get(&app_core_mode);
    if (request->payload.command.flag && mode!=APP_CORE_STARTUP && mode!=APP_CORE_NORMAL)
        return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}
