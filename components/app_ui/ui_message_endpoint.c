#include "app_ui_internal.h"
#include "ui_model.h"
#include "ui_camera_messages.h"
#include "ui_input_messages.h"
#include "ui_menu_messages.h"
#include "ui_preferences.h"
#include "ui_frames.h"
#include "ui_bench.h"
#include "app_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <stdatomic.h>
static atomic_bool started,closing,admission_closed;
static bool cleanup_pending;
static uint16_t subscribed;
static uint32_t frame_generation;
static unsigned frame_count, window_count;
static int64_t frame_window;
bool app_ui_preferences_quiesce(uint32_t timeout_ms)
{ return ui_preferences_quiesce(timeout_ms); }
bool app_ui_debug_quiesce(uint32_t timeout_ms)
{
#if CONFIG_REMOTE_DBG_SIM
    return ui_bench_quiesce(timeout_ms);
#else
    (void)timeout_ms;return true;
#endif
}

/* Kept separate from the router: it knows only the message contract and UI
 * model/renderer. It does not contain Camera/Wi-Fi lifecycle or private heads. */
static esp_err_t handle(app_message_t *message, app_message_t *reply, bool *deferred)
{
    switch (message->type) {
    case APP_MESSAGE_DISPLAY_BENCH:
#if CONFIG_REMOTE_DBG_SIM
        return ui_bench_message(message,reply);
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case APP_MESSAGE_UI_MENU_ACTION:
        return ui_menu_message_apply(message,reply);
    case APP_MESSAGE_UI_PROPERTY_STATUS:
        return ui_property_message(message,reply);
    case APP_MESSAGE_UI_PREFERENCES:
        return ui_preferences_message(message,reply,deferred);
    case APP_MESSAGE_INPUT_STATE:
        return ui_input_message_apply(message);
    case APP_MESSAGE_CAMERA_PROPERTIES: case APP_MESSAGE_CAMERA_STATE:
    case APP_MESSAGE_CAMERA_CAPABILITIES: case APP_MESSAGE_CAMERA_COMMAND_STATUS:
        return ui_camera_message_apply(message);
    case APP_MESSAGE_WIFI_NETWORK_CHANGED:
        app_ui_set_wifi_rssi(-127);
        return ESP_OK;
    case APP_MESSAGE_WIFI_STATUS:
        app_ui_set_wifi_info(message->payload.network.config.ssid,
            message->payload.network.config.password,
            message->payload.network.config.show_password,
            message->payload.network.online ? message->payload.network.address : "",
            message->payload.network.config.default_password);
        app_ui_refresh_wifi_info();
        return ESP_OK;
    case APP_MESSAGE_WIFI_RSSI:
        app_ui_set_wifi_rssi(message->payload.peer.rssi);
        return ESP_OK;
    case APP_MESSAGE_UI_STATUS: {
        ui_menu_snapshot(&reply->payload.ui);
        return ESP_OK;
    }
    case APP_MESSAGE_CAMERA_FRAME: {
        int64_t entered = esp_timer_get_time();
        esp_err_t error = ui_frames_handle(message);
        if (error == ESP_OK) {
            if (frame_generation != message->generation) {
                frame_generation = message->generation;
                frame_count = window_count = 0;
                frame_window = entered;
            }
            ++frame_count; ++window_count;
            int64_t shown = esp_timer_get_time();
            if (frame_count == 1 || shown-frame_window >= 5000000) {
                size_t size = 0; app_message_lease_data(message->lease, &size);
                ESP_LOGI("app_ui", "LIVEVIEW frames=%u fps=%.2f JPEG=%u read=%lums display=%ldms stack_free=%u",
                    frame_count, (double)window_count*1000000/(shown-frame_window > 0 ? shown-frame_window : 1),
                    (unsigned)size, (unsigned long)message->payload.command.duration_ms,
                    (long)((shown-entered)/1000), (unsigned)uxTaskGetStackHighWaterMark(NULL));
                frame_window = shown; window_count = 0;
            }
        }
        return error;
    }
    case APP_MESSAGE_DISPLAY_FAULT:
#if CONFIG_REMOTE_DBG_SIM
        return app_ui_test_display_fault(message->payload.command.value);
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    default:
        return ESP_ERR_NOT_SUPPORTED;
    }
}

static void message_task(void *context)
{
    (void)context;
    for (;;) {
        ui_frames_flush();
        if (!ui_frames_can_receive()) { vTaskDelay(pdMS_TO_TICKS(25)); continue; }
        app_message_t message = {0}, reply = {0};
        esp_err_t received = app_console_receive(APP_ENDPOINT_UI, &message, atomic_load(&closing) ? 0 : 250);
        if (received == ESP_ERR_INVALID_STATE) break;
        if (received != ESP_OK) {
            if (atomic_load(&closing) && !ui_frames_pending()) break;
            vTaskDelay(pdMS_TO_TICKS(25));continue;
        }
        bool deferred=false;
        if (atomic_load(&closing) || atomic_load(&admission_closed)) {
            reply.result=message.type==APP_MESSAGE_CAMERA_FRAME ? ui_frames_drop(&message,ESP_ERR_INVALID_STATE) : ESP_ERR_INVALID_STATE;
        } else reply.result = handle(&message, &reply, &deferred);
        if (!deferred && (message.flags & APP_MESSAGE_REQUEST)) app_console_reply(&message, &reply);
        app_message_release(&message);
    }
    while (ui_frames_pending()) { ui_frames_flush();if (ui_frames_pending()) vTaskDelay(pdMS_TO_TICKS(25)); }
    /* Remain a live source until completion metadata was sent/retired. Pending
     * JPEG leases are caller-owned and were released above, including drops. */
    app_console_endpoint_stop(APP_ENDPOINT_UI);
    atomic_store(&started,false);vTaskDeleteWithCaps(NULL);
}

static bool cleanup_start(void)
{
    cleanup_pending=!ui_preferences_quiesce(500);return !cleanup_pending;
}
static esp_err_t start_failed(esp_err_t error,bool owns_endpoint)
{
    if (owns_endpoint) app_console_endpoint_stop(APP_ENDPOINT_UI);
    cleanup_start();return error;
}
esp_err_t app_ui_messages_start(void)
{
    if (atomic_load(&started) || atomic_load(&admission_closed) || ui_frames_pending()) return ESP_ERR_INVALID_STATE;
    if (cleanup_pending && !cleanup_start()) return ESP_ERR_INVALID_STATE;
    atomic_store(&closing,false);
    esp_err_t initialized=ui_preferences_start();
    if (initialized!=ESP_OK) return initialized;
    const app_endpoint_config_t config = {16, 2};
    esp_err_t error=app_console_endpoint_register(APP_ENDPOINT_UI,&config);
    if (error!=ESP_OK) return start_failed(error,false);
    app_console_status_t router;app_console_get_status(&router);
    if (!router.subscriptions_frozen) subscribed=0;
    const app_message_type_t events[]={APP_MESSAGE_WIFI_STATUS,APP_MESSAGE_INPUT_STATE,
        APP_MESSAGE_WIFI_RSSI,APP_MESSAGE_WIFI_NETWORK_CHANGED,APP_MESSAGE_CAMERA_STATE,
        APP_MESSAGE_CAMERA_PROPERTIES,APP_MESSAGE_CAMERA_CAPABILITIES,
        APP_MESSAGE_CAMERA_COMMAND_STATUS,APP_MESSAGE_CAMERA_FRAME};
    for (unsigned i=0;error==ESP_OK && i<sizeof(events)/sizeof(events[0]);++i) {
        if (subscribed&(1u<<i)) continue;
        error=app_console_subscribe(events[i],APP_ENDPOINT_UI);
        if (error==ESP_OK) subscribed|=1u<<i;
    }
    if (error!=ESP_OK) return start_failed(error,true);
    atomic_store(&started,true);
    if (xTaskCreatePinnedToCoreWithCaps(message_task,"ui_endpoint",32768,
        NULL,4,NULL,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS) {
        atomic_store(&started,false);return start_failed(ESP_ERR_NO_MEM,true);
    }
    return ESP_OK;
}

void app_ui_close_admission(void) { atomic_store(&admission_closed,true); }
bool app_ui_messages_quiesce(uint32_t timeout_ms)
{
    atomic_store(&closing,true);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&started)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return !ui_frames_pending();
}
