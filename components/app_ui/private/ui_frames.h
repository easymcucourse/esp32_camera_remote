#pragma once
#include "app_message.h"
/* UI owner only. At most two producer slots need result metadata; retrying a
 * full control inbox never retains the JPEG lease or adds another worker. */
bool ui_frames_can_receive(void);
void ui_frames_flush(void);
esp_err_t ui_frames_handle(const app_message_t *message);

/* UI owner only. Drop without rendering, queue completion; caller still owns
 * the incoming JPEG lease and releases it exactly once. */
bool ui_frames_pending(void);
esp_err_t ui_frames_drop(const app_message_t *message,esp_err_t error);
