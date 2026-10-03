#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "gamepad_input.h"
esp_err_t maint_mode_start(void);
bool maint_mode_command(int argc,char **argv);
void maint_mode_poll(void);
bool maint_mode_is_on(void);
void maint_mode_on_camera_session(bool open);
void maint_mode_touch(void);
esp_err_t maint_mode_request_off(void);
/* Terminal shutdown: rejects new enables, stops HTTP and releases owned lease.
 * Caller runs on an internal stack; never call from the HTTP server task. */
bool maint_mode_quiesce(unsigned timeout_ms);
esp_err_t maint_mode_upload_begin(void);
void maint_mode_upload_end(void);
bool maint_mode_upload_active(void);
bool maint_mode_is_shutting_down(void);
/* Nonblocking input handoff; called before normal menu/camera dispatch. */
bool maint_mode_gamepad(pad_action_t action);
/* HTTP service owns authentication; accessors copy under its mutex. */
esp_err_t maint_web_start(void);
esp_err_t maint_web_stop(void);
void maint_web_pin(char pin[7]);
