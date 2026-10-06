#pragma once
#include <stdbool.h>
#include "pad_types.h"
// Transitional declarations for old main callers; remove after Input/UART/core
// migration. Camera endpoint and sole owner use these inside this component.
// Queued controls never expose a backend/channel or mutable state.
void camera_pair_start(void);
void camera_jpeg_start(void);
void camera_stop_request(void);
// Queue one exposure-mode step: -1 previous, +1 next.
void camera_mode_step(int direction);
void camera_focus_mode_step(int direction);
// Nonblocking input API; socket writes remain in the camera owner task.
bool camera_focus_ready(void);
void camera_focus_step(int direction); // +1 near, -1 far
void camera_focus_cancel(void);
/* Thread-safe, nonblocking handoff; only the camera owner sends PTP/IP. */
void camera_gamepad_caps(gamepad_caps_t *out);
bool camera_gamepad_action(pad_action_t action);
bool camera_controller_admitting(void);

// Only succeeds when no camera task is active. Clears sony_remote NVS namespace.
/* Maintenance worker only. Blocks new starts, stops/drains the socket owner,
 * then reserves the idle camera. On failure the start gate is released. */
typedef struct { bool busy, stopped, session; int last_io; char phase[96]; } camera_debug_status_t;
void camera_debug_get_status(camera_debug_status_t *out);
bool camera_display_begin(void);
bool camera_display_ready(void);
void camera_display_end(void);
