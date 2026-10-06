#pragma once
#include "network_config.h"
typedef struct { const char *ssid,*password;bool has_channel;double channel;bool has_show_password,show_password; } maint_wifi_patch_t;
/* Failed validation leaves out unchanged; field points to a static error key. */
network_cfg_error_t maint_wifi_patch(const network_config_t *current,const maint_wifi_patch_t *patch,
                                 unsigned max_channel,network_config_t *out,const char **field);
