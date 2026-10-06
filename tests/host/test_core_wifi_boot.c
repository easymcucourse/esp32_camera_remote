#include "app_core_wifi_boot.h"
#include <assert.h>
#include <string.h>
static const char *scenario;
static int object;
static network_config_t saved,applied;
static unsigned starts,reads;
static bool is(const char *name) { return !strcmp(scenario,name); }
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
app_wifi_result_t wifi_esp32_create(app_wifi_t **out)
{ if(is("create_failed"))return APP_WIFI_NO_MEMORY;*out=(app_wifi_t *)&object;return APP_WIFI_OK; }
app_wifi_result_t app_wifi_saved_config_read(app_wifi_t *wifi,network_config_t *out)
{ assert(wifi==(app_wifi_t *)&object);++reads;*out=saved;return is("read_failed")?APP_WIFI_IO:APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_get(app_wifi_t *wifi,network_config_t *out)
{ assert(wifi==(app_wifi_t *)&object);*out=saved;return is("get_failed")?APP_WIFI_IO:APP_WIFI_OK; }
app_wifi_result_t app_wifi_init(app_wifi_t *wifi)
{ assert(wifi==(app_wifi_t *)&object);return is("init_failed")?APP_WIFI_STATE:APP_WIFI_OK; }
app_wifi_result_t app_wifi_get_status(app_wifi_t *wifi,app_wifi_status_t *out)
{ assert(wifi==(app_wifi_t *)&object);out->max_channel=11;return APP_WIFI_OK; }
app_wifi_result_t app_wifi_start(app_wifi_t *wifi,const network_config_t *config)
{ assert(wifi==(app_wifi_t *)&object);++starts;applied=*config;return is("start_failed")?APP_WIFI_TIMEOUT:APP_WIFI_OK; }
/* Deliberately no saved-config writer, console, task or message implementation. */
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];network_config_make_default(&saved);
    if(is("invalid"))saved.channel=0;
    network_config_t original=saved;
    esp_err_t created=app_core_wifi_create();
    if(is("create_failed")){assert(created==ESP_ERR_NO_MEM && !reads && !starts);return 0;}
    assert(created==ESP_OK && reads==1 && app_core_wifi_service()==(app_wifi_t *)&object);
    esp_err_t started=app_core_wifi_start();
    if(is("init_failed"))assert(started==ESP_ERR_INVALID_STATE && !starts);
    else if(is("start_failed"))assert(started==ESP_ERR_TIMEOUT && starts==1);
    else {
        network_config_t expected;network_config_make_default(&expected);
        assert(started==ESP_OK && starts==1 && network_config_equal(&applied,&expected));
    }
    assert(!memcmp(&saved,&original,sizeof saved));
    return 0;
}
