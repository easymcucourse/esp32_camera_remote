#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#define APP_UI_API_VERSION 1
/* Core-only lifecycle. Initialize once; errors require shutdown/reboot, never
 * partial retry. Retains panel/framebuffers for the fixed maintenance picture. */
esp_err_t app_ui_init(const char *ssid, const char *password);
/* Core startup metadata; subsequent normal updates arrive through messages. */
void app_ui_set_wifi_info(const char *ssid, const char *password, bool show_password, const char *ip, bool default_password);
/* Start after renderer initialization and before normal producers. Registers
 * fixed subscriptions and loads read-only boot preferences. One boot lifetime. */
esp_err_t app_ui_messages_start(void);
/* Irreversibly reject ordinary UI/JPEG admission while allowing lease returns. */
void app_ui_close_admission(void);
/* Join endpoint after normal producers/benchmark stop; timeout retains owner
 * for retry. Does not close renderer. Successful repeats succeed. */
bool app_ui_messages_quiesce(uint32_t timeout_ms);
/* Retire read-only preference cache access; no worker/queue or persistence. */
bool app_ui_preferences_quiesce(uint32_t timeout_ms);
void app_ui_debug_ready(void);
bool app_ui_debug_quiesce(uint32_t timeout_ms);
/* After endpoint and all model writers join: close renderer/refresh, drain
 * readers and free JPEG cache. Timeout stays closed; retry or reboot. */
bool app_ui_renderer_quiesce(uint32_t timeout_ms);
/* Same preconditions. Permanently clear/freeze normal model, then publish only
 * MAINTENANCE. Idempotent on success; failure keeps normal drawing closed. */
esp_err_t app_ui_enter_maintenance(uint32_t timeout_ms);
/* Health signal remains valid after normal model clear. */
bool app_ui_display_failed(void);
