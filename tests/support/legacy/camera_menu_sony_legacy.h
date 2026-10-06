#pragma once
#include "camera_menu.h"
/* Temporary raw dataset entry, removed from the protocol-free kernel API. */
extern const uint16_t camera_menu_codes[CAMERA_MENU_COUNT];
bool camera_menu_snapshot(camera_menu_t *menu, const uint8_t *data, size_t size, uint32_t now);
