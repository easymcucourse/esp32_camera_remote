#pragma once
#include "app_message.h"
/* Pure message validation/model application, plus connection renderer chosen
 * by semantic stage. No Camera facade/backend/Sony includes. */
esp_err_t ui_camera_message_apply(const app_message_t *message);
bool ui_camera_generation_accept(uint32_t generation);
