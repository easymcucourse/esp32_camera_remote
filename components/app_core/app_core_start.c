#include "app_core.h"
#include "app_core_services.h"
#include "app_core_camera.h"
#include "app_console.h"
#include "esp_timer.h"
#include "app_ui.h"
#include "esp_chip_info.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "app_core_wifi_boot.h"
#include "app_camera.h"
#include "app_core_maintenance.h"
#include "app_maintenance.h"
#include "app_core_ota_health.h"
#include "app_core_mode.h"
#include "app_core_network.h"
#include "app_core_shutdown.h"

#define BOOT_TRY(operation) do { result=(operation); if (result!=ESP_OK) { \
    ESP_LOGE("core", "Boot stage %s failed: %s", #operation, esp_err_to_name(result)); \
    goto failed; } } while (0)
/* One startup caller. Failed boot closes HTTP and drains normal owners before
 * returning the original error. Failed drains remain closed until fatal reset. */
static bool attempted;
esp_err_t app_core_start(void)
{
    if (attempted) return ESP_ERR_INVALID_STATE;
    attempted=true;
    esp_err_t result=ESP_OK;
    bool ui_attempted=false;
    app_core_mode_boot_begin(&app_core_mode);
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    ESP_LOGI("remote", "ESP32-S3 LCD-7B | cores=%d | PSRAM=%u bytes",
             chip.cores, (unsigned)esp_psram_get_size());
    ESP_LOGI("remote", "Touch disabled; PTP/IP live-view JPEG enabled");
    // Preserve existing NVS; do not silently erase it on an incompatible layout.
    esp_err_t nvs_result = nvs_flash_init();
    if (nvs_result != ESP_OK) ESP_LOGE("remote", "NVS unavailable: %s; preserving contents and using default Wi-Fi", esp_err_to_name(nvs_result));
    BOOT_TRY(app_core_wifi_create());
    BOOT_TRY(app_core_network_bind(app_core_wifi_service()));
    network_config_t ap; app_core_wifi_config(&ap);
    ui_attempted=true;
    BOOT_TRY(app_ui_init(ap.ssid, ap.show_password ? ap.password : "********"));
    app_ui_set_wifi_info(ap.ssid, ap.password, ap.show_password, NULL,network_config_uses_default_password(&ap));
    BOOT_TRY(heap_caps_check_integrity_all(true) ? ESP_OK : ESP_FAIL);
    ESP_LOGI("remote", "UI init stack headroom=%u bytes",
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    /* Prepare the physical I2C device before AP allocation. Normal provider
     * tasks and routing start only after the startup trigger is listening. */
    BOOT_TRY(app_core_input_providers_prepare());
    BOOT_TRY(app_core_wifi_start());
    BOOT_TRY(app_core_maintenance_init(app_core_wifi_service()));
    BOOT_TRY(app_maintenance_trigger_open());
    /* Claims can arrive before Camera exists. Finish any creation already in
     * progress, skip subsequent stages, then release the exclusive drain. */
    if (app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP)
        BOOT_TRY(app_core_messages_start());
    if (app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP)
        BOOT_TRY(app_core_network_messages_start());
    if (app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP)
        BOOT_TRY(app_core_input_providers_start());
    if (app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP)
        BOOT_TRY(app_core_camera_boot());
    /* Normal boot requires the UART subscriptions to exist before freezing.
     * An early maintenance claim may skip UART and has no normal lifetime. */
    if (app_core_console_ready()) app_console_freeze_subscriptions();
    else if (app_core_mode_get(&app_core_mode)==APP_CORE_STARTUP) {
        result=ESP_ERR_INVALID_STATE;goto failed;
    }
#if CONFIG_REMOTE_DBG_SIM
    app_ui_debug_ready();
#endif
    ESP_LOGI("remote", "READY: LCD connection screen, UART 115200");
    app_core_ota_startup_ready(app_core_wifi_service());
    const app_core_health_ops_t health = {
        .health_tick = app_core_ota_health,
        .maintenance_quiesce = app_core_maintenance_stop,
        .camera_drain = app_core_camera_quiesce,
    };
    BOOT_TRY(app_core_health_start(&health));
    app_core_mode_boot_end(&app_core_mode);
    // IDF deletes the caller main task after return, reclaiming the font-init stack.
    return ESP_OK;
failed:
    app_core_mode_restart(&app_core_mode);
    app_core_mode_boot_end(&app_core_mode);
    if (ui_attempted) {
        bool http=app_core_maintenance_stop(3000);
        bool normal=app_core_normal_stop();
        ESP_LOGE("core","Boot failed: maintenance_closed=%d normal_stopped=%d",http,normal);
    }
    return result;
}
