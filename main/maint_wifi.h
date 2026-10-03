#pragma once
#include "wifi_config.h"
typedef struct { const char *ssid,*password;bool has_channel;double channel; } maint_wifi_patch_t;
/* Failed validation leaves out unchanged; field points to a static error key. */
wifi_cfg_error_t maint_wifi_patch(const app_wifi_config_t *current,const maint_wifi_patch_t *patch,
                                 unsigned max_channel,app_wifi_config_t *out,const char **field);
