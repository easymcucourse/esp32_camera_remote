#include "maint_wifi.h"
#include <string.h>
wifi_cfg_error_t maint_wifi_patch(const app_wifi_config_t *current,const maint_wifi_patch_t *patch,
                                 unsigned max_channel,app_wifi_config_t *out,const char **field)
{
    if (!current || !patch || !out || !field) return WIFI_CFG_INVALID;
    app_wifi_config_t next=*current;wifi_cfg_error_t error=WIFI_CFG_OK;
    *field="config";
    if (patch->ssid) {
        *field="ssid";error=wifi_config_check_ssid(patch->ssid);
        if (error!=WIFI_CFG_OK) return error;
        strcpy(next.ssid,patch->ssid);
    }
    if (patch->password) {
        *field="password";error=wifi_config_check_password(patch->password);
        if (error!=WIFI_CFG_OK) return error;
        strcpy(next.password,patch->password);
    }
    if (patch->has_channel) {
        *field="channel";
        if (!(patch->channel>=1 && patch->channel<=max_channel && patch->channel<=13) ||
            patch->channel!=(unsigned)patch->channel) return WIFI_CFG_CHANNEL_RANGE;
        next.channel=(uint8_t)patch->channel;
    }
    error=wifi_config_check(&next,max_channel);
    if (error!=WIFI_CFG_OK) return error;
    *out=next;*field="";return WIFI_CFG_OK;
}
