#include <assert.h>
#include <string.h>
#include "maint_wifi.h"
int main(void)
{
    network_config_t current,out,sentinel;network_config_make_default(&current);
    memset(&sentinel,0x55,sizeof(sentinel));out=sentinel;const char *field;
    maint_wifi_patch_t patch={.ssid="new network",.password="1234567"};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_PASSWORD_TOO_SHORT);
    assert(!strcmp(field,"password") && !memcmp(&out,&sentinel,sizeof(out)));
    patch=(maint_wifi_patch_t){.has_channel=true,.channel=1.5};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_CHANNEL_RANGE);
    patch.channel=-1;assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_CHANNEL_RANGE);
    patch.channel=12;assert(maint_wifi_patch(&current,&patch,11,&out,&field)==NETWORK_CFG_CHANNEL_RANGE);
    patch.channel=11;assert(maint_wifi_patch(&current,&patch,11,&out,&field)==NETWORK_CFG_OK);
    assert(out.channel==11 && !strcmp(out.password,current.password) && out.show_password==current.show_password);
    char ssid[34];memset(ssid,'a',33);ssid[33]=0;patch=(maint_wifi_patch_t){.ssid=ssid};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_SSID_TOO_LONG);
    ssid[32]=0;assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_OK);
    patch=(maint_wifi_patch_t){0};assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_OK && network_config_equal(&current,&out));
    patch=(maint_wifi_patch_t){.has_show_password=true,.show_password=!current.show_password};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==NETWORK_CFG_OK && out.show_password!=current.show_password && network_config_network_equal(&current,&out));
    return 0;
}
