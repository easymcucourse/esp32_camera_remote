#pragma once
#include <stdbool.h>
#include "gamepad_input.h"
// Compatibility API implemented by camera_controller.c.
// Initialize the console once at boot, before starting camera requests.
// Start and mode-step calls enqueue/create work; sockets stay with the camera task.
void camera_pair_start(void);
void camera_jpeg_start(void);
void camera_pair_console_init(void);
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

// Only succeeds when no camera task is active. Clears sony_remote NVS namespace.
bool camera_forget_pairing(void);
/* Maintenance worker only. Blocks new starts, stops/drains the socket owner,
 * then reserves the idle camera. On failure the start gate is released. */
bool camera_maintenance_acquire(uint32_t timeout_ms);
void camera_maintenance_release(void);
typedef struct { bool busy, stopped, session; int last_io; char phase[96]; } camera_debug_status_t;
void camera_debug_get_status(camera_debug_status_t *out);
