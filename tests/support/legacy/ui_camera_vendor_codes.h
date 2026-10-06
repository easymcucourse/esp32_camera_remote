#pragma once
/* Historical vendor-code fixtures only; never a production UI contract. */
#include "camera_settings.h"
static const uint16_t camera_extra_codes[CAMERA_EXTRA_COUNT] = {
    0xd211, 0x5013, 0xd21b, 0xd201, 0xd22c, 0xd262, 0xd20f, 0xd21c, 0xd210
};
void app_ui_set_camera_property(uint16_t code,uint32_t value);
void app_ui_set_command_status(uint16_t code,unsigned status);
