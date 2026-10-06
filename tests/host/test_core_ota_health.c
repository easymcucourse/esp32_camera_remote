#include "app_core_ota_health.h"
#include "esp_ota_ops.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>
static const char *scenario;
static int64_t now;
static unsigned confirmations,rollbacks,restarts,closed,drained,status_reads,heap_reads;
static int wifi_context;
static const esp_partition_t running={"ota_0",6*1024*1024};
static bool is(const char *name) { return !strcmp(scenario,name); }
void core_boot_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "fake"; }
int64_t esp_timer_get_time(void) { return now; }
const esp_partition_t *esp_ota_get_running_partition(void) { return &running; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state)
{ assert(partition==&running);*state=is("not_pending")?ESP_OTA_IMG_VALID:ESP_OTA_IMG_PENDING_VERIFY;return ESP_OK; }
const char *app_maintenance_ota_boot_status(void) { return "fake"; }
bool app_ui_display_failed(void) { return is("display"); }
bool heap_caps_check_integrity_all(bool print) { assert(print);++heap_reads;return !is("heap"); }
app_wifi_result_t app_wifi_get_status(app_wifi_t *wifi,app_wifi_status_t *status)
{
    assert(wifi==(app_wifi_t *)&wifi_context);++status_reads;
    *status=(app_wifi_status_t){.started=true,.online=!is("ap_offline") && !is("close_fail")};
    if(!is("ap_no_address"))strcpy(status->address,"192.168.4.1");
    return is("ap_error")?APP_WIFI_IO:APP_WIFI_OK;
}
bool app_core_maintenance_stop(unsigned timeout) { assert(timeout==3000);++closed;return !is("close_fail"); }
bool app_core_camera_quiesce(uint32_t timeout) { assert(timeout==3000);++drained;return true; }
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void) { ++rollbacks;return ESP_FAIL; }
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) { ++confirmations;return is("confirm_fail")?ESP_FAIL:ESP_OK; }
void esp_restart(void) { ++restarts; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];
    app_core_ota_health();assert(!status_reads);
    app_core_ota_startup_ready((app_wifi_t *)&wifi_context);
    now=59999999;app_core_ota_health();
    if(is("heap"))assert(rollbacks==1 && restarts==1 && drained==1 && !confirmations);
    else assert(!rollbacks && !confirmations);
    if(is("heap"))return 0;
    // Tick spacing must not extend the original 60s health decision boundary.
    now=60000000;app_core_ota_health();assert(!confirmations);
    now=61000000;app_core_ota_health();
    if(is("display")||is("not_pending"))assert(!heap_reads && !status_reads && !rollbacks && !confirmations);
    else if(is("confirm")) {
        assert(confirmations==1 && !rollbacks);now=62000000;app_core_ota_health();assert(confirmations==1 && status_reads==2);
    } else if(is("confirm_fail"))assert(confirmations==1 && rollbacks==1 && restarts==1 && drained==1 && closed==1);
    else if(is("close_fail")) {
        assert(!confirmations && rollbacks==1 && restarts==1 && closed==1 && !drained);
    } else assert(!confirmations && rollbacks==1 && restarts==1 && drained==1 && closed==1);
    return 0;
}
