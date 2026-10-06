#include "app_core_shutdown.h"
#include "app_core.h"
#include "app_core_services.h"
#include "app_core_mode.h"
#include "app_core_camera.h"
#include "app_core_messages.h"
#include "app_core_network.h"
#include "app_camera.h"
#include "app_console.h"
#include "app_input.h"
#include "app_ui.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

static bool stopped;
static atomic_bool closing,closed;
void app_core_normal_close(void)
{
    if (app_core_mode_booting(&app_core_mode)) return;
    bool expected=false;
    if (!atomic_compare_exchange_strong(&closing,&expected,true)) return;
    app_camera_close_admission();
    /* Zero-budget quiesce closes each admission without waiting. Preserve
     * Camera/router until release actions and JPEG completion metadata finish.
     * Preferences/config remain until admitted normal writes finish. */
    (void)app_console_uart_quiesce(0);
    (void)app_input_quiesce(0);
    (void)app_ui_debug_quiesce(0);
    app_ui_close_admission();
    atomic_store(&closed,true);
}
bool app_core_normal_stop(void)
{
    if (app_core_mode_booting(&app_core_mode)) return false;
    if (stopped) return true;
    app_core_normal_close();
    int64_t close_deadline=esp_timer_get_time()+1000000;
    while (!atomic_load(&closed)) {
        if (esp_timer_get_time()>=close_deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    /* Reply to queued session/normal requests before stopping the camera; its
     * in-flight session request must not wait for a health consumer that is now
     * draining it. Exclusive mode rejects new business requests here. */
    app_core_messages_poll();
    bool uart=app_console_uart_quiesce(1000);
    bool input=app_input_quiesce(1000)==ESP_OK;
    bool providers=app_core_input_providers_stop(1000)==ESP_OK;
    bool bench=app_ui_debug_quiesce(1000);
    bool camera=bench && app_core_camera_quiesce(3000);
    bool camera_endpoint=camera && app_core_camera_messages_quiesce(1000);
    bool network=uart && input && providers && bench && camera_endpoint &&
        app_core_network_quiesce(3000)==ESP_OK;
    bool preferences=app_ui_preferences_quiesce(1000);
    bool ui=network && preferences && app_ui_messages_quiesce(1000);
    bool renderer=ui && app_ui_renderer_quiesce(1000);
    bool router=renderer && app_core_messages_quiesce(1000);
    stopped=uart && input && providers && bench && camera &&
        camera_endpoint && network && preferences && ui && renderer && router;
    ESP_LOGI("core","Normal stop: uart=%d input=%d providers=%d bench=%d camera=%d endpoint=%d network=%d preferences=%d ui=%d renderer=%d router=%d",
        uart,input,providers,bench,camera,camera_endpoint,network,preferences,ui,renderer,router);
    return stopped;
}
