#include "preferences_store.h"
#include "nvs.h"
#include <string.h>
esp_err_t preferences_store_set(const char *key,unsigned value)
{
    if(!key || ((!strcmp(key,"info") && value>2) || (!strcmp(key,"pad") && value>1)) ||
        (strcmp(key,"info") && strcmp(key,"pad")))return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;esp_err_t error=nvs_open("ui_prefs",NVS_READWRITE,&nvs);
    if(error!=ESP_OK)return error;
    error=nvs_set_u8(nvs,key,value);
    if(error==ESP_OK)error=nvs_set_u8(nvs,"schema",PREFERENCES_CONFIG_VERSION);
    if(error==ESP_OK)error=nvs_commit(nvs);
    nvs_close(nvs);return error;
}
