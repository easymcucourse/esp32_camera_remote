#include "app_core_ota_health.h"
#include "app_core.h"
#include "app_core_services.h"
#include "app_core_camera.h"
#include "app_core_maintenance.h"
#include "app_maintenance_ota.h"
#include "app_ui.h"
#include "ota_health.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_system.h"
static app_wifi_t *health_wifi;
static bool ready,pending;
static int64_t ready_at,next_check;
void app_core_ota_startup_ready(app_wifi_t *wifi)
{
    health_wifi=wifi;
    esp_ota_img_states_t state;const esp_partition_t *run=esp_ota_get_running_partition();
    pending=run && esp_ota_get_state_partition(run,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY;
    ready=true;ready_at=esp_timer_get_time();
    next_check=0;
    ESP_LOGI("maint_ota","Boot running=%s status=%s; confirmation after 60s healthy runtime",run?run->label:"unknown",app_maintenance_ota_boot_status());
}
static void rollback(const char *reason)
{
    ESP_LOGE("maint_ota","OTA self-test failed (%s); requesting rollback",reason);
    if (app_core_maintenance_stop(3000)) app_core_camera_quiesce(3000);
    esp_err_t err=esp_ota_mark_app_invalid_rollback_and_reboot();
    /* This API returns only on failure. Do not continue normal execution after
     * failed self-test; the bootloader re-evaluates the recorded slot states. */
    ESP_LOGE("maint_ota","Rollback API returned: %s; restarting for bootloader recovery",esp_err_to_name(err));
    esp_restart();
}
void app_core_ota_health(void)
{
    int64_t now=esp_timer_get_time();if (!ready || !pending || now<next_check) return;next_check=now+1000000;
    if (app_ui_display_failed()) return; /* Normal fatal-display path closes maintenance before resetting; unconfirmed image rolls back. */
    bool heap_ok=heap_caps_check_integrity_all(true);
    app_wifi_status_t ap={0};
    bool ap_up=health_wifi && app_wifi_get_status(health_wifi,&ap)==APP_WIFI_OK && ap.started && ap.online && ap.address[0];
    ota_health_action_t action=ota_health_decide(now-ready_at,heap_ok,ap_up);
    if (action==OTA_HEALTH_ROLLBACK) {
        rollback(heap_ok?"hotspot unavailable after 60s":"heap integrity");
        return;
    }
    if (action==OTA_HEALTH_CONFIRM) {
        esp_err_t err=esp_ota_mark_app_valid_cancel_rollback();
        if (err==ESP_OK) { pending=false;ESP_LOGI("maint_ota","OTA confirmed after 60s healthy runtime"); }
        else { ESP_LOGE("maint_ota","OTA confirmation failed: %s",esp_err_to_name(err));rollback("confirmation failed"); }
    }
}
