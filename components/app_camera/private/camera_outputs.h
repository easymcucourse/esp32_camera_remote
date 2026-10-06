#pragma once
#include "camera_properties.h"
#include "app_message.h"
#include "camera_menu_properties.h"
/* Sole Camera owner builds a copied UI snapshot. Nothing exposes backend or
 * borrowed property choices outside app_camera; notifications use Console. */
bool camera_view_build(const camera_properties_t *properties, const setting_control_t *mode,
    const camera_menu_t *menu, const gamepad_caps_t *caps, app_camera_view_t *view);
esp_err_t camera_outputs_properties(uint32_t generation, const app_camera_view_t *view);
esp_err_t camera_outputs_state(uint32_t generation, const app_camera_status_t *status);
esp_err_t camera_outputs_command(uint32_t generation, app_camera_control_t control, unsigned status);
