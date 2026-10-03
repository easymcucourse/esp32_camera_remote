#include <assert.h>
#include <string.h>
#include "maint_wifi.h"
int main(void)
{
    app_wifi_config_t current,out,sentinel;wifi_config_make_default(&current);
    memset(&sentinel,0x55,sizeof(sentinel));out=sentinel;const char *field;
    maint_wifi_patch_t patch={.ssid="new network",.password="1234567"};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_PASSWORD_TOO_SHORT);
    assert(!strcmp(field,"password") && !memcmp(&out,&sentinel,sizeof(out)));
    patch=(maint_wifi_patch_t){.has_channel=true,.channel=1.5};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_CHANNEL_RANGE);
    patch.channel=-1;assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_CHANNEL_RANGE);
    patch.channel=12;assert(maint_wifi_patch(&current,&patch,11,&out,&field)==WIFI_CFG_CHANNEL_RANGE);
    patch.channel=11;assert(maint_wifi_patch(&current,&patch,11,&out,&field)==WIFI_CFG_OK);
    assert(out.channel==11 && !strcmp(out.password,current.password) && out.show_password==current.show_password);
    char ssid[34];memset(ssid,'a',33);ssid[33]=0;patch=(maint_wifi_patch_t){.ssid=ssid};
    assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_SSID_TOO_LONG);
    ssid[32]=0;assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_OK);
    patch=(maint_wifi_patch_t){0};assert(maint_wifi_patch(&current,&patch,13,&out,&field)==WIFI_CFG_OK && wifi_config_equal(&current,&out));
    return 0;
}
