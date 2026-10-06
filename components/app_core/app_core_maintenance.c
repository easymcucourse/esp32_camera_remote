#include "app_core_maintenance.h"
#include "app_core_mode.h"
#include "app_core_shutdown.h"
#include "app_core_settings.h"
#include "app_core_factory_reset.h"
#include "app_maintenance.h"
#include "app_maintenance_web.h"
#include "app_maintenance_ota.h"
#include "app_restart.h"
#include "app_ui.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

static app_wifi_t *network;
static atomic_bool uploading;
static atomic_uint config_token;
static bool available(void *context)
{ (void)context;return app_core_mode_get(&app_core_mode)==APP_CORE_MAINTENANCE; }
static bool closing(void *context)
{ (void)context;return app_core_mode_get(&app_core_mode)==APP_CORE_RESTART; }
static void touch(void *context) { (void)context; } /* No idle TTL or return to normal. */
static bool prepare(void *context)
{ return available(context) && app_restart_prepare(); }
static void cancel(void *context) { (void)context;app_restart_cancel(); }
static bool commit(void *context,unsigned delay)
{
    if (!available(context)) return false;
    if (!app_restart_commit(delay)) app_core_mode_restart(&app_core_mode);
    return true; /* Failed scheduler commit still has the health reboot path. */
}
static esp_err_t begin(void *context)
{
    if (!available(context)) return ESP_ERR_INVALID_STATE;
    bool expected=false;
    return atomic_compare_exchange_strong(&uploading,&expected,true) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
static void end(void *context) { (void)context;atomic_store(&uploading,false); }
static esp_err_t settings_read(void *context,app_maintenance_settings_t *settings)
{ return available(context) ? app_core_settings_read(settings) : ESP_ERR_INVALID_STATE; }
static esp_err_t settings_write(void *context,const app_maintenance_settings_t *settings)
{
    return available(context) && !atomic_load(&uploading) ?
        app_core_settings_write(settings) : ESP_ERR_INVALID_STATE;
}
static esp_err_t factory_reset(void *context,bool all)
{
    return available(context) && !atomic_load(&uploading) ?
        app_core_factory_reset(network,all) : ESP_ERR_INVALID_STATE;
}
static void wifi_committed(void *context,uint32_t token)
{ if (available(context) && token) atomic_store(&config_token,token); }
static void reboot(void *context)
{ (void)context;app_core_mode_restart(&app_core_mode); }
static bool exclusive(void *context,uint32_t timeout_ms)
{
    (void)context;
    if (!app_core_mode_request_maintenance(&app_core_mode)) return false;
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (app_core_mode_booting(&app_core_mode)) {
        if (esp_timer_get_time()>=deadline) { reboot(NULL);return false; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (app_core_mode_get(&app_core_mode)!=APP_CORE_ACTIVATING) return false;
    app_core_normal_close();
    while (app_core_mode_get(&app_core_mode)==APP_CORE_ACTIVATING) {
        if (esp_timer_get_time()>=deadline) { reboot(NULL);return false; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return app_core_mode_get(&app_core_mode)==APP_CORE_MAINTENANCE;
}
esp_err_t app_core_maintenance_init(app_wifi_t *wifi)
{
    if (!wifi) return ESP_ERR_INVALID_ARG;
    if (network) return ESP_ERR_INVALID_STATE;
    const app_maintenance_ota_ops_t ota={prepare,cancel,commit,begin,end,closing,NULL};
    esp_err_t error=app_maintenance_ota_init(&ota);
    if (error!=ESP_OK) return error;
    const app_maintenance_web_ops_t web={available,touch,prepare,cancel,commit,settings_read,settings_write,factory_reset,wifi_committed,NULL};
    error=app_maintenance_web_init(wifi,&web);
    if (error!=ESP_OK) return error;
    const app_maintenance_system_ops_t system={exclusive,reboot,NULL};
    error=app_maintenance_init(wifi,&system);
    if (error==ESP_OK) network=wifi;
    return error;
}
void app_core_maintenance_poll(void)
{
    if (app_core_mode_get(&app_core_mode)==APP_CORE_MAINTENANCE) {
        uint32_t token=atomic_load(&config_token);
        if (!token) return;
        app_wifi_result_t completed=APP_WIFI_PENDING;
        app_wifi_result_t result=app_wifi_config_result(network,token,&completed);
        if (result==APP_WIFI_PENDING) return;
        atomic_store(&config_token,0);
        if (result==APP_WIFI_OK && completed==APP_WIFI_OK) (void)commit(NULL,1500);
        else app_restart_cancel();
        return;
    }
    if (app_core_mode_get(&app_core_mode)!=APP_CORE_ACTIVATING || app_core_mode_booting(&app_core_mode)) return;
    /* HTTP is waiting on exclusive(). It must remain alive throughout this
     * drain. Full routes remain closed until both modes are committed. */
    if (!network || !app_core_normal_stop() || app_core_mode_get(&app_core_mode)!=APP_CORE_ACTIVATING ||
        app_ui_enter_maintenance(1000)!=ESP_OK || app_core_mode_get(&app_core_mode)!=APP_CORE_ACTIVATING ||
        app_wifi_config_start(network)!=APP_WIFI_OK ||
        app_maintenance_activate()!=ESP_OK || !app_core_mode_activate(&app_core_mode))
        reboot(NULL);
}
esp_err_t app_core_enter_normal(void)
{
    if (!app_core_mode_enter_normal(&app_core_mode)) return ESP_ERR_INVALID_STATE;
    app_maintenance_trigger_close();
    if (app_maintenance_stop()!=ESP_OK) { reboot(NULL);return ESP_FAIL; }
    return ESP_OK;
}
bool app_core_maintenance_stop(unsigned timeout_ms)
{
    app_core_mode_restart(&app_core_mode);
    bool http=app_maintenance_stop()==ESP_OK;
    /* Active maintenance may have restarted its sole config worker. Keep AP,
     * wait for admitted storage work before any restart/OTA rollback. */
    bool config=!network || app_wifi_config_quiesce(network,timeout_ms)==APP_WIFI_OK;
    return http && config;
}
