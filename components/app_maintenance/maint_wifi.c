#include "maint_wifi.h"
#include <string.h>
network_cfg_error_t maint_wifi_patch(const network_config_t *current,const maint_wifi_patch_t *patch,
                                 unsigned max_channel,network_config_t *out,const char **field)
{
    if (!current || !patch || !out || !field) return NETWORK_CFG_INVALID;
    network_config_t next=*current;network_cfg_error_t error=NETWORK_CFG_OK;
    *field="config";
    if (patch->ssid) {
        *field="ssid";error=network_config_check_ssid(patch->ssid);
        if (error!=NETWORK_CFG_OK) return error;
        strcpy(next.ssid,patch->ssid);
    }
    if (patch->password) {
        *field="password";error=network_config_check_password(patch->password);
        if (error!=NETWORK_CFG_OK) return error;
        strcpy(next.password,patch->password);
    }
    if (patch->has_channel) {
        *field="channel";
        if (!(patch->channel>=1 && patch->channel<=max_channel && patch->channel<=13) ||
            patch->channel!=(unsigned)patch->channel) return NETWORK_CFG_CHANNEL_RANGE;
        next.channel=(uint8_t)patch->channel;
    }
    if(patch->has_show_password)next.show_password=patch->show_password;
    error=network_config_check(&next,max_channel);
    if (error!=NETWORK_CFG_OK) return error;
    *out=next;*field="";return NETWORK_CFG_OK;
}
