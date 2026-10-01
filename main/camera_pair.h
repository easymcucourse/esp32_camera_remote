#pragma once
#include <stdbool.h>
// Compatibility API implemented by camera_controller.c.
// Initialize the console once at boot, before starting camera requests.
// Start and mode-step calls enqueue/create work; sockets stay with the camera task.
void camera_pair_start(void);
void camera_jpeg_start(void);
void camera_pair_console_init(void);
// Queue one exposure-mode step: -1 previous, +1 next.
void camera_mode_step(int direction);
// Nonblocking input API; socket writes remain in the camera owner task.
bool camera_focus_ready(void);
void camera_focus_step(int direction); // +1 near, -1 far
void camera_focus_cancel(void);

// Only succeeds when no camera task is active. Clears sony_remote NVS namespace.
void camera_forget_pairing(void);
