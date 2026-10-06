#pragma once
#include <stdbool.h>
#include <stdint.h>

#define PAD_START (1u << 3)
#define PAD_SELECT (1u << 0)
#define PAD_UP (1u << 4)
#define PAD_RIGHT (1u << 5)
#define PAD_DOWN (1u << 6)
#define PAD_LEFT (1u << 7)
#define PAD_DIRECTIONS (PAD_UP | PAD_RIGHT | PAD_DOWN | PAD_LEFT)
#define PAD_L1 (1u << 10)
#define PAD_R1 (1u << 11)
#define PAD_Y (1u << 12) /* DS4 Triangle */
#define PAD_X (1u << 15) /* DS4 Square */
#define PAD_A (1u << 14) /* DS4 Cross */
#define PAD_B (1u << 13) /* DS4 Circle */
#define PAD_TOUCH (1u << 17) /* DS4 touchpad click: LCD information level. */
#define PAD_SHOULDERS (PAD_L1 | PAD_R1)

typedef enum { PAD_LENS_UNKNOWN, PAD_LENS_POWER_ZOOM, PAD_LENS_NON_POWER_ZOOM } pad_lens_t;
typedef struct {
    bool session, settings, mf_known, mf, zoom_known, zoom_enabled;
    bool recording_known, recording, record_pending;
    pad_lens_t lens;
    uint32_t generation; /* Changes on every session or safety cancellation. */
} gamepad_caps_t;
/* This project's PZ lens is explicitly confirmed by the user, not inferred
 * from a disabled/missing digital zoom capability property. Camera acceptance
 * still determines whether the requested motor operation succeeds. */
static inline bool gamepad_zoom_available(const gamepad_caps_t *caps)
{ return caps->lens==PAD_LENS_POWER_ZOOM || (caps->zoom_known && caps->zoom_enabled); }

typedef struct {
    bool connected;
    uint32_t buttons;
    int8_t rx, ry;
    uint8_t lt, rt, battery;
} gamepad_snapshot_t;
typedef enum {
    PAD_ACTION_S1, PAD_ACTION_S2, PAD_ACTION_RECORD, PAD_ACTION_ZOOM,
    PAD_ACTION_MF_STEP, PAD_ACTION_MF_CANCEL, PAD_ACTION_RELEASE_ALL,
    PAD_ACTION_MODE_NEXT, PAD_ACTION_FOCUS_MODE_NEXT, PAD_ACTION_UI_TOGGLE,
    PAD_ACTION_RECORD_UNAVAILABLE, PAD_ACTION_MENU_MOVE, PAD_ACTION_MENU_STEP,
    PAD_ACTION_MENU_CONFIRM, PAD_ACTION_MENU_BACK, PAD_ACTION_UI_INFO_NEXT,
    PAD_ACTION_MAINT_TOGGLE
} pad_action_type_t;
typedef struct { pad_action_type_t type; int value; uint32_t generation; } pad_action_t;
