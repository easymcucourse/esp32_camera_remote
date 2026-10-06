#include "app_core_settings.h"
#include "nvs.h"
#include <assert.h>
#include <string.h>
static const char *scenario;
static unsigned opens,closes,sets,commits;
static bool is(const char *name) { return !strcmp(scenario,name); }
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *handle)
{ assert(!strcmp(name,"ui_prefs") && (mode==NVS_READONLY || mode==NVS_READWRITE));++opens;*handle=1;return is("open_failed")?ESP_FAIL:is("missing") && mode==NVS_READONLY?ESP_ERR_NVS_NOT_FOUND:ESP_OK; }
void nvs_close(nvs_handle_t handle) { assert(handle==1);++closes; }
esp_err_t nvs_get_u8(nvs_handle_t handle,const char *key,uint8_t *value)
{
    assert(handle==1);if(is("read_failed"))return ESP_FAIL;
    if(!strcmp(key,"schema")) { if(is("legacy"))return ESP_ERR_NVS_NOT_FOUND;*value=is("future")?2:1; }
    else if(!strcmp(key,"pad"))*value=1;
    else { assert(!strcmp(key,"info"));*value=2; }
    return ESP_OK;
}
esp_err_t nvs_set_u8(nvs_handle_t handle,const char *key,uint8_t value)
{ assert(handle==1);++sets;if(!strcmp(key,"schema"))assert(value==1);else if(!strcmp(key,"pad"))assert(value==1);else assert(!strcmp(key,"info") && value==2);return is("set_failed")?ESP_FAIL:ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t handle) { assert(handle==1);++commits;return is("commit_failed")?ESP_FAIL:ESP_OK; }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];app_maintenance_settings_t settings={99,99,99};
    assert(app_core_settings_read(NULL)==ESP_ERR_INVALID_ARG && !opens);
    esp_err_t read=app_core_settings_read(&settings);
    if(is("future"))assert(read==ESP_ERR_NOT_SUPPORTED && settings.version==99 && closes==1);
    else if(is("open_failed") || is("read_failed"))assert(read==ESP_FAIL && settings.version==99);
    else if(is("missing"))assert(read==ESP_OK && settings.version==1 && settings.controller==0 && settings.info_level==0 && !closes);
    else assert(read==ESP_OK && settings.version==1 && settings.controller==1 && settings.info_level==2 && closes==1);
    settings=(app_maintenance_settings_t){1,1,2};
    assert(app_core_settings_write(NULL)==ESP_ERR_INVALID_ARG);
    settings.version=2;assert(app_core_settings_write(&settings)==ESP_ERR_INVALID_ARG);
    settings.version=1;esp_err_t write=app_core_settings_write(&settings);
    if(is("open_failed"))assert(write==ESP_FAIL && !sets && !commits && !closes);
    else if(is("set_failed"))assert(write==ESP_FAIL && sets==1 && !commits);
    else if(is("commit_failed"))assert(write==ESP_FAIL && sets==3 && commits==1);
    else assert(write==ESP_OK && sets==3 && commits==1);
    return 0;
}
