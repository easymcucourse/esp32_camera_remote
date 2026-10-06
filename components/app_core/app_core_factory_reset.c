#include "app_core_factory_reset.h"
#include "app_core_factory.h"
#include "camera_identity.h"
#include "preferences_store.h"
#include "esp_log.h"

typedef struct { app_wifi_t *wifi; bool all; } reset_context_t;
/* The composition callback has already stopped Camera/UI and frozen config.
 * No reservation or runtime state is acquired/released by these primitives. */
static bool reserved(void *context) { (void)context;return true; }
static void release(void *context) { (void)context; }
static bool save(void *context,const network_config_t *config)
{ return app_wifi_saved_config_write(((reset_context_t *)context)->wifi,config)==APP_WIFI_OK; }
static bool forget(void *context)
{ return !((reset_context_t *)context)->all || camera_identity_forget(); }
static bool preferences(void *context)
{ return !((reset_context_t *)context)->all || preferences_store_reset()==ESP_OK; }

esp_err_t app_core_factory_reset(app_wifi_t *wifi,bool all)
{
    if (!wifi) return ESP_ERR_INVALID_ARG;
    app_wifi_result_t frozen=app_wifi_config_freeze(wifi,5000);
    if (frozen!=APP_WIFI_OK) return frozen==APP_WIFI_TIMEOUT ? ESP_ERR_TIMEOUT : ESP_ERR_INVALID_STATE;
    network_config_t current;
    if (app_wifi_saved_config_read(wifi,&current)!=APP_WIFI_OK) {
        app_wifi_config_resume(wifi);return ESP_FAIL;
    }
    reset_context_t context={wifi,all};
    const factory_reset_ops_t operations={reserved,release,save,forget,preferences};
    factory_reset_result_t result=factory_reset_all(&current,&operations,&context);
    ESP_LOGI("factory_reset","Maintenance reset scope=%s result=%u",all ? "all" : "wifi",(unsigned)result);
    if (result==FACTORY_RESET_OK) return ESP_OK;
    /* Persistence across namespaces is not atomic. Transaction attempts only
     * Wi-Fi rollback; keep isolation even if Camera/UI records were changed. */
    app_wifi_config_resume(wifi);
    return ESP_FAIL;
}
