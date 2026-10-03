#include <inttypes.h>
#include "board_7b.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "camera_pair.h"
#include "atom_link.h"
#include "wifi_menu_ui.h"
#include "ui_preferences.h"
#include "maint_mode.h"
#include "app_restart.h"
#include "maint_ota.h"
#include "display_bench.h"

static void health_task(void *arg)
{
    (void)arg;
    int64_t next_memory_log = 0;
    while (true) {
        maint_ota_health();
        bool display_failed=board_7b_display_failed();
        if (display_failed || app_restart_due()) {
            ESP_LOGI("remote", "%s restart: closing maintenance and draining camera",display_failed?"Display":"Web");
            bool maintenance_closed=maint_mode_quiesce(3000);
            bool drained = maintenance_closed && camera_maintenance_acquire(3000);
            ESP_LOGI("remote", "%s restart: camera_drained=%d; NVS preserved; maintenance_closed=%d",
                     display_failed?"Display":"Web",drained,maintenance_closed);
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

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI("remote", "ESP32-S3 LCD-7B | cores=%d | PSRAM=%u bytes",
             chip.cores, (unsigned)esp_psram_get_size());
    ESP_LOGI("remote", "Touch disabled; PTP/IP live-view JPEG enabled");
    // Preserve existing NVS; do not silently erase it on an incompatible layout.
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result != ESP_OK) ESP_LOGE("remote", "NVS unavailable: %s; preserving contents and using default Wi-Fi", esp_err_to_name(nvs_result));
    wifi_ap_load_config();
    app_wifi_config_t ap; wifi_ap_get_config(&ap);
    ESP_ERROR_CHECK(board_7b_init(ap.ssid, ap.show_password ? ap.password : "********"));
    board_7b_set_wifi_info(ap.ssid, ap.password, ap.show_password, NULL,wifi_config_uses_default_password(&ap));
    ESP_ERROR_CHECK(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL);
    ESP_LOGI("remote", "UI init stack headroom=%u bytes",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    wifi_menu_ui_start();
    ESP_ERROR_CHECK(ui_preferences_start());
    // Create the UI queue before input; retain the hardware-verified ATOM/Wi-Fi startup order.
    atom_link_start();
    wifi_ap_start();
    ESP_ERROR_CHECK(maint_mode_start());
    camera_pair_console_init();
    camera_jpeg_start();
    display_bench_ready();
    ESP_LOGI("remote", "READY: LCD connection screen, UART 115200");
    maint_ota_startup_ready();
    ESP_ERROR_CHECK(xTaskCreateWithCaps(health_task, "health", 4096, NULL, 2, NULL,
                                      MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)==pdPASS?ESP_OK:ESP_ERR_NO_MEM);
    // IDF deletes the main task after return, reclaiming its large font initialization stack.
}
