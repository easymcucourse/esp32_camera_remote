#pragma once
#include <stdbool.h>
#include <stdint.h>
/* UI-private gate. Each admitted draw/notification pairs enter/leave. Closing
 * is irreversible until reboot; in-flight users finish, never revoked. */
bool ui_render_enter(void);
void ui_render_leave(void);
bool ui_render_stopping(void);
bool ui_render_idle(void);
void ui_render_refresh_set(bool active);
bool ui_render_close(int64_t deadline_us);
