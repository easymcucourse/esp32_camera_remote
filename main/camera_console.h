#pragma once
// Called once after the controller creates its command queue.
void camera_console_init(void);
// Internal controller entry point for the UART task.
void camera_stop_request(void);
