#include "app_core.h"
#include "app_core_services.h"
#include "app_console.h"
#include "app_core_messages.h"
#include "app_core_mode.h"
#include "app_core_maintenance.h"
#include "app_core_shutdown.h"
#include "app_core_network.h"
#include "app_core_camera.h"
#include "app_restart.h"
#include "app_ui.h"
#include "app_input.h"
#include <inttypes.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"

static app_core_health_ops_t health_ops;
static bool started;

static void health_task(void *arg)
{
    (void)arg;
    int64_t next_memory_log = 0;
    while (true) {
        if (app_core_mode_booting(&app_core_mode)) {
            vTaskDelay(pdMS_TO_TICKS(10));continue;
        }
        app_core_maintenance_poll();
        app_core_mode_value_t mode=app_core_mode_get(&app_core_mode);
        if (mode==APP_CORE_STARTUP || mode==APP_CORE_NORMAL) app_core_messages_poll();
        health_ops.health_tick();
        bool display_failed=app_ui_display_failed();
        if (display_failed || app_restart_due() || app_core_mode_get(&app_core_mode)==APP_CORE_RESTART) {
            app_core_mode_restart(&app_core_mode);
            ESP_LOGI("remote", "%s restart: closing maintenance and draining camera",display_failed?"Display":"Web");
            bool maintenance_closed=health_ops.maintenance_quiesce(3000);
            bool normal_stopped=app_core_normal_stop();
            ESP_LOGI("remote","Restart: maintenance_closed=%d normal_stopped=%d; NVS preserved",maintenance_closed,normal_stopped);
            /* health has an internal-RAM stack; never restart from a PSRAM decoder stack. */
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        }
        int64_t now = esp_timer_get_time();
        if (now >= next_memory_log) {
            ESP_LOGI("remote", "uptime=%" PRId64 "s free_internal=%u free_psram=%u min_internal=%u min_psram=%u largest_internal=%u largest_psram=%u",
                 esp_timer_get_time() / 1000000,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
            next_memory_log = now + 10000000;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t app_core_health_start(const app_core_health_ops_t *ops)
{
    if (!ops || !ops->health_tick || !ops->maintenance_quiesce || !ops->camera_drain)
        return ESP_ERR_INVALID_ARG;
    if (started) return ESP_ERR_INVALID_STATE;
    health_ops = *ops;
    if (xTaskCreateWithCaps(health_task, "health", 4096, NULL, 2, NULL,
                          MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) != pdPASS)
        return ESP_ERR_NO_MEM;
    started = true;
    return ESP_OK;
}
