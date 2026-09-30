#pragma once
void camera_pair_start(void);
void camera_jpeg_start(void);
void camera_pair_console_init(void);
// Queue one exposure-mode step: -1 previous, +1 next.
void camera_mode_step(int direction);
