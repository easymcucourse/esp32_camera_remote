#pragma once
#include "app_message.h"
/* Component-private intake. It never owns a backend or JPEG buffer. */
esp_err_t camera_endpoint_start(void);
void camera_endpoint_stop(void);
bool camera_endpoint_quiesce(uint32_t timeout_ms);
bool camera_endpoint_frame_result(app_message_t *message);
bool camera_controller_active(void);
uint32_t camera_controller_lifetime(void);
uint32_t camera_controller_frame_generation(void);
void camera_controller_setting(unsigned property, int direction);
void camera_controller_network_changed(uint32_t generation);
