#pragma once
#include "app_wifi.h"
#include "esp_err.h"
#define APP_WIFI_MESSAGES_API_VERSION 1
/* Composition owner binds one live Wi-Fi object. With TCP capability, two
 * independent single-owner channels serve command/event streams. Socket I/O
 * runs in separate workers; control/status dispatch stays responsive. Stop
 * cancels all channels, drains leases and joins all tasks before success.
 * Timeout retains ownership for retry. Stop before destroying the object.
 * Duplicate start returns STATE. */
esp_err_t app_wifi_messages_start(app_wifi_t *wifi);
esp_err_t app_wifi_messages_stop(uint32_t timeout_ms);
/* Transitional adapter for the old composition/camera path; the final normal
 * Camera path uses WIFI_SELECT_CAMERA message instead of calling this. */
void app_wifi_messages_select_camera(const uint8_t mac[6]);
