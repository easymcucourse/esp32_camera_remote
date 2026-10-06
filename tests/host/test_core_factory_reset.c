#include "app_core_factory_reset.h"
#include "camera_identity.h"
#include "preferences_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *scenario;
static int context;
static network_config_t saved,original;
static bool frozen;
static unsigned writes,erases,resets,resumes;
static bool is(const char *name) { return !strcmp(scenario,name); }
void core_boot_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
app_wifi_result_t app_wifi_config_freeze(app_wifi_t *wifi,uint32_t timeout)
{
    assert(wifi==(app_wifi_t *)&context && timeout==5000 && !frozen);
    if (is("freeze_timeout")) return APP_WIFI_TIMEOUT;
    if (is("freeze_failed")) return APP_WIFI_STATE;
    frozen=true;return APP_WIFI_OK;
}
void app_wifi_config_resume(app_wifi_t *wifi)
{ assert(wifi==(app_wifi_t *)&context && frozen);frozen=false;++resumes; }
app_wifi_result_t app_wifi_saved_config_read(app_wifi_t *wifi,network_config_t *config)
{ assert(wifi==(app_wifi_t *)&context && frozen);*config=saved;return is("read_failed")?APP_WIFI_IO:APP_WIFI_OK; }
app_wifi_result_t app_wifi_saved_config_write(app_wifi_t *wifi,const network_config_t *config)
{
    assert(wifi==(app_wifi_t *)&context && frozen);++writes;
    if ((is("write_failed") && writes==1) || (is("rollback_failed") && writes==2)) return APP_WIFI_IO;
    saved=*config;return APP_WIFI_OK;
}
bool camera_identity_forget(void)
{ assert(frozen && writes==1);++erases;return !is("identity_failed") && !is("rollback_failed"); }
esp_err_t preferences_store_reset(void)
{ assert(frozen && writes==1 && erases==1);++resets;return is("preferences_failed")?ESP_FAIL:ESP_OK; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];
    network_config_make_default(&saved);strcpy(saved.ssid,"saved-newer-than-active");original=saved;
    assert(app_core_factory_reset(NULL,true)==ESP_ERR_INVALID_ARG);
    esp_err_t result=app_core_factory_reset((app_wifi_t *)&context,!is("wifi"));
    if (is("wifi") || is("all")) {
        network_config_t defaults;network_config_make_default(&defaults);
        assert(result==ESP_OK && network_config_equal(&saved,&defaults) && frozen && !resumes);
        assert(writes==1 && erases==!is("wifi") && resets==!is("wifi"));
    } else {
        assert(result==(is("freeze_timeout")?ESP_ERR_TIMEOUT:is("freeze_failed")?ESP_ERR_INVALID_STATE:ESP_FAIL));
        assert(!frozen && resumes==(!is("freeze_timeout") && !is("freeze_failed")));
        if (is("read_failed") || is("freeze_failed") || is("freeze_timeout")) assert(!writes && !erases && !resets);
        else {
            assert(writes==2);
            if (!is("rollback_failed")) assert(network_config_equal(&saved,&original));
            assert(erases==!is("write_failed"));
            assert(resets==is("preferences_failed"));
        }
    }
    puts("Exclusive factory reset freezes sole config writer and preserves failure/rollback semantics");
    return 0;
}
