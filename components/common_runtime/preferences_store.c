#include "preferences_store.h"
#include "nvs.h"
#include <string.h>
/* Persist only ui_prefs keys; normal application reads them on next boot.
 * No UI runtime API or normal preference writer. Core admits maintenance only
 * after normal owners drain; its serialized HTTP caller owns writes/resets.
 * All three keys are staged through one handle and committed once.
 * Missing schema is legacy v1; unknown versions are preserved and rejected. */
esp_err_t preferences_store_load(preferences_config_t *settings)
{
    if(!settings)return ESP_ERR_INVALID_ARG;
    preferences_config_t saved={PREFERENCES_CONFIG_VERSION,0,0};
    nvs_handle_t nvs;esp_err_t error=nvs_open("ui_prefs",NVS_READONLY,&nvs);
    if(error==ESP_ERR_NVS_NOT_FOUND) { *settings=saved;return ESP_OK; }
    if(error!=ESP_OK)return error;
    uint8_t version=1,value=0;
    error=nvs_get_u8(nvs,"schema",&version);
    if(error==ESP_ERR_NVS_NOT_FOUND)error=ESP_OK;
    if(error==ESP_OK && version!=PREFERENCES_CONFIG_VERSION)error=ESP_ERR_NOT_SUPPORTED;
    if(error==ESP_OK) {
        esp_err_t read=nvs_get_u8(nvs,"pad",&value);
        if(read==ESP_OK && value<=1)saved.controller=value;
        else if(read!=ESP_OK && read!=ESP_ERR_NVS_NOT_FOUND)error=read;
        read=nvs_get_u8(nvs,"info",&value);
        if(read==ESP_OK && value<=2)saved.info_level=value;
        else if(read!=ESP_OK && read!=ESP_ERR_NVS_NOT_FOUND)error=read;
    }
    nvs_close(nvs);
    if(error==ESP_OK)*settings=saved;
    return error;
}
esp_err_t preferences_store_write(const preferences_config_t *settings)
{
    if(!settings || settings->version!=PREFERENCES_CONFIG_VERSION || settings->controller>1 || settings->info_level>2)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;esp_err_t error=nvs_open("ui_prefs",NVS_READWRITE,&nvs);
    if(error!=ESP_OK)return error;
    error=nvs_set_u8(nvs,"info",settings->info_level);
    if(error==ESP_OK)error=nvs_set_u8(nvs,"pad",settings->controller);
    if(error==ESP_OK)error=nvs_set_u8(nvs,"schema",settings->version);
    if(error==ESP_OK)error=nvs_commit(nvs);
    nvs_close(nvs);return error;
}

esp_err_t preferences_store_reset(void)
{
    const preferences_config_t defaults={PREFERENCES_CONFIG_VERSION,0,0};
    return preferences_store_write(&defaults);
}
