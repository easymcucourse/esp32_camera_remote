#pragma once
#include "setting_control.h"
#include <stddef.h>

enum {
    MENU_SHUTTER, MENU_APERTURE, MENU_ISO, MENU_EV, MENU_WB, MENU_FOCUS, MENU_METER,
    MENU_ASPECT, MENU_DRIVE, MENU_EFFECT, MENU_DRO, MENU_AF_AREA, MENU_WL_FLASH,
    MENU_WB_TEMP, MENU_WB_AB, MENU_WB_GM, CAMERA_MENU_COUNT
};
extern const uint16_t camera_menu_codes[CAMERA_MENU_COUNT];
typedef struct {
    setting_control_t control;
    uint16_t type;
    bool relative, writable;
    int remaining, sent_direction;
    uint32_t actual, baseline, deadline;
    bool awaiting;
    setting_status_t status;
} camera_menu_item_t;
typedef struct { camera_menu_item_t items[CAMERA_MENU_COUNT]; } camera_menu_t;
typedef struct { unsigned index; uint16_t code, type; uint32_t value; bool relative; } camera_menu_write_t;
/* Socket-owner only; no borrowed descriptor or optimistic actual values. */
bool camera_menu_snapshot(camera_menu_t *menu, const uint8_t *data, size_t size, uint32_t now);
bool camera_menu_step(camera_menu_t *menu, unsigned index, int steps, bool wrap);
bool camera_menu_next(camera_menu_t *menu, unsigned index, uint32_t now, camera_menu_write_t *write);
void camera_menu_response(camera_menu_t *menu, unsigned index, bool accepted);
void camera_menu_cancel(camera_menu_t *menu);
bool camera_menu_pending(const camera_menu_t *menu);
setting_status_t camera_menu_status(const camera_menu_t *menu, unsigned index);
bool camera_menu_target(const camera_menu_t *menu, unsigned index, uint32_t *target);
