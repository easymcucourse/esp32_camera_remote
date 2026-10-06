#include "app_maintenance.h"
#include "app_maintenance_web.h"
#include "maintenance_trigger.h"
#include "maintenance_web_internal.h"
#include <stdatomic.h>

static app_maintenance_system_ops_t system_ops;
static atomic_bool initialized;
static atomic_uint phase=APP_MAINTENANCE_IDLE;

esp_err_t app_maintenance_init(app_wifi_t *wifi,const app_maintenance_system_ops_t *system)
{
    if (!wifi || !system || !system->request_exclusive || !system->request_reboot)
        return ESP_ERR_INVALID_ARG;
    if (atomic_load(&initialized)) return ESP_ERR_INVALID_STATE;
    if (app_wifi_api_version(wifi)!=APP_WIFI_API_VERSION ||
        !(app_wifi_capabilities(wifi)&APP_WIFI_CAP_AP)) return ESP_ERR_NOT_SUPPORTED;
    if (!maintenance_web_uses_network(wifi)) return ESP_ERR_INVALID_STATE;
    system_ops=*system;
    atomic_store(&initialized,true);
    return ESP_OK;
}

esp_err_t app_maintenance_trigger_open(void)
{
    if (!atomic_load(&initialized)) return ESP_ERR_INVALID_STATE;
    unsigned expected=APP_MAINTENANCE_IDLE;
    if (!atomic_compare_exchange_strong(&phase,&expected,APP_MAINTENANCE_TRIGGER))
        return ESP_ERR_INVALID_STATE;
    esp_err_t error=app_maintenance_web_start();
    if (error!=ESP_OK) atomic_store(&phase,APP_MAINTENANCE_CLOSED);
    return error;
}

void app_maintenance_trigger_close(void)
{
    if (atomic_load(&initialized)) atomic_store(&phase,APP_MAINTENANCE_CLOSED);
}

esp_err_t app_maintenance_activate(void)
{
    if (!atomic_load(&initialized)) return ESP_ERR_INVALID_STATE;
    unsigned expected=APP_MAINTENANCE_ACTIVATING;
    return atomic_compare_exchange_strong(&phase,&expected,APP_MAINTENANCE_ACTIVE) ?
        ESP_OK : ESP_ERR_INVALID_STATE;
}

void app_maintenance_get_status(app_maintenance_status_t *status)
{
    if (!status) return;
    status->initialized=atomic_load(&initialized);
    status->phase=(app_maintenance_phase_t)atomic_load(&phase);
}

esp_err_t app_maintenance_stop(void)
{
    app_maintenance_trigger_close();
    return app_maintenance_web_stop();
}

bool maintenance_trigger_route(httpd_req_t *request)
{
    if (!atomic_load(&initialized)) {
        httpd_resp_set_status(request,"409 Conflict");
        httpd_resp_send(request,"maintenance not initialized",HTTPD_RESP_USE_STRLEN);
        return false;
    }
    unsigned expected=APP_MAINTENANCE_TRIGGER;
    if (atomic_compare_exchange_strong(&phase,&expected,APP_MAINTENANCE_ACTIVATING)) {
        bool accepted=system_ops.request_exclusive(system_ops.context,35000);
        if (accepted && atomic_load(&phase)==APP_MAINTENANCE_ACTIVE) {
            httpd_resp_set_status(request,"302 Found");
            httpd_resp_set_hdr(request,"Location","/");
            httpd_resp_set_hdr(request,"Cache-Control","no-store");
            httpd_resp_set_hdr(request,"Connection","close");
            httpd_resp_send(request,"",0);
        } else {
            atomic_store(&phase,APP_MAINTENANCE_CLOSED);
            if (accepted) system_ops.request_reboot(system_ops.context);
            httpd_resp_set_status(request,"409 Conflict");
            httpd_resp_set_hdr(request,"Connection","close");
            httpd_resp_send(request,"maintenance unavailable; restart to retry",HTTPD_RESP_USE_STRLEN);
        }
        return false;
    }
    if (expected==APP_MAINTENANCE_ACTIVE) return true;
    httpd_resp_set_status(request,"409 Conflict");
    httpd_resp_set_hdr(request,"Connection","close");
    httpd_resp_send(request,"maintenance unavailable; restart to retry",HTTPD_RESP_USE_STRLEN);
    return false;
}
