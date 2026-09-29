#include <inttypes.h>
#include "board_7b.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi_ap.h"
#include "camera_pair.h"

void app_main(void)
{
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI("remote", "ESP32-S3 LCD-7B | cores=%d | PSRAM=%u bytes",
             chip.cores, (unsigned)esp_psram_get_size());
    ESP_LOGI("remote", "Touch disabled; PTP/IP live-view JPEG enabled");
    // Preserve existing NVS; do not silently erase it on an incompatible layout.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(board_7b_init(AP_SSID, AP_PASSWORD));
    wifi_ap_start();
    camera_pair_console_init();
    camera_jpeg_start();
    ESP_LOGI("remote", "READY: LCD connection screen, UART 115200");
    while (true) {
        wifi_ap_log_clients();
        ESP_LOGI("remote", "uptime=%" PRId64 "s free_internal=%u free_psram=%u",
                 esp_timer_get_time() / 1000000,
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
