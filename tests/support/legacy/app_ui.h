#pragma once
#define APP_UI_API_VERSION 1
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Core marks Debug benchmark admission ready after initial Camera startup. */
void app_ui_debug_ready(void);
bool app_ui_debug_quiesce(uint32_t timeout_ms);
/* Metadata only; connection screen worker owns actual drawing. */
void app_ui_set_maint_text(const char *text);
void app_ui_request_maint_screen(bool active);
void app_ui_set_sim(bool active);
void app_ui_set_info_level(unsigned level);

// One-time initialization. On failure, app_main aborts; no partial-init retry.
esp_err_t app_ui_init(const char *ssid, const char *password);
/* Core registers the UI endpoint after init and before input/Camera startup.
 * Owns preferences and Wi-Fi menu workers; the endpoint retains the renderer's
 * CPU1/priority4/PSRAM stack policy. */
esp_err_t app_ui_messages_start(void);
/* Core exclusive claim: immediately reject ordinary UI/JPEG work while keeping
 * the endpoint alive for lease returns and completion metadata until quiesce.
 * Irreversible for this boot; subsequent message start is forbidden. */
void app_ui_close_admission(void);
/* Core-only message worker stop. First quiesce Input/Camera producers and UI
 * bench/menu/preferences. Drops queued JPEGs with completion metadata, retains
 * endpoint/worker on timeout. Does not stop connection refresh/renderer. */
bool app_ui_messages_quiesce(uint32_t timeout_ms);
/* Core-only after all producers/UI workers/messages stopped. Closes drawing
 * irreversibly, waits refresh/draw/notification users, releases JPEG cache.
 * Panel/framebuffers/fonts/mutex remain for the fixed maintenance picture. */
bool app_ui_renderer_quiesce(uint32_t timeout_ms);
/* Same preconditions. Only MAINTENANCE is published; successful repeats are
 * idempotent. Failure keeps normal drawing closed; retry or reboot. */
esp_err_t app_ui_enter_maintenance(uint32_t timeout_ms);
/* Core-only preference worker shutdown. UI endpoint remains active; new
 * preference operations are rejected. Does not stop menu/renderer workers. */
bool app_ui_preferences_quiesce(uint32_t timeout_ms);
/* Core-only menu task close/drain; Wi-Fi/System jobs require domain quiesce. */
// Call only before starting the JPEG worker or after it has drained.
esp_err_t app_ui_show_connection(const char *status);
void app_ui_set_wifi_info(const char *ssid, const char *password, bool show_password, const char *ip, bool default_password);
void app_ui_refresh_wifi_info(void); /* Nonblocking notification to the render worker. */
// Refresh connection-screen peripheral status; Atom loss also disconnects DS4.
void app_ui_set_atom_status(bool atom_online, bool controller_online);
/* Battery is the input protocol's 0..10 level, or 255 unknown. */
void app_ui_set_controller_battery(unsigned level, bool xbox);
void app_ui_set_atom_protocol(bool mismatch, unsigned gimbal_link_state);
// Live-view status shown in the upper-right corner.
void app_ui_set_wifi_rssi(int rssi);
void app_ui_set_camera_info(const char *model, const char *firmware);
void app_ui_set_exposure_mode(uint32_t mode);
void app_ui_set_camera_property(uint16_t code, uint32_t value);
/* Status numbers: idle=0, pending=1, applied=2, rejected=3, timeout=4, accepted=5.
 * Atomic publication; rendering stays with the LCD worker. */
void app_ui_set_command_status(uint16_t code, unsigned status);
void app_ui_set_recording_status(bool known, bool recording);
bool app_ui_toggle_settings_mode(void);
bool app_ui_settings_mode(void);
void app_ui_menu_move(int direction);
unsigned app_ui_menu_selected(void);
bool app_ui_extra_menu_active(void);
void app_ui_extra_menu_open(bool active);
void app_ui_extra_menu_move(int direction);
bool app_ui_extra_menu_exit_selected(void);
/* Maintenance menu: 0 off, 1 confirm, 2 pending, 3 on, 4 failed. */
void app_ui_set_maint_menu(unsigned state);
typedef struct {
    bool active;
    unsigned selected;
    char lines[9][80], footer[96];
} app_ui_wifi_menu_view_t;
void app_ui_set_wifi_menu(const app_ui_wifi_menu_view_t *view);
uint32_t app_ui_connection_generation(void);
/* Seven primary parameter rows followed by nine extra parameter rows. */
void app_ui_set_menu_item(unsigned index, bool writable, unsigned status,
                           bool target_valid, uint32_t target);
typedef struct { uint32_t actual, target; unsigned status; bool writable, target_valid; } app_ui_extra_status_t;
bool app_ui_get_extra_status(unsigned index, app_ui_extra_status_t *out);
/* Single JPEG worker only. INVALID_RESPONSE drops this image; INVALID_STATE
 * requires recovery. No framebuffer is published on a decode error. */
esp_err_t app_ui_show_jpeg(const uint8_t *jpeg, size_t length);
/* Serialized with rendering; at most three panel recovery attempts. */
esp_err_t app_ui_recover_display(void);
/* Fatal recovery failure; app_main drains the camera and restarts on internal stack. */
bool app_ui_display_failed(void);
typedef struct { unsigned fps_tenths, battery, focus; int32_t ev; bool settings, failed, default_password; } app_ui_status_t;
void app_ui_get_status(app_ui_status_t *out);
esp_err_t app_ui_test_display_fault(unsigned mode);
/* Developer-only synthetic input; caller must own the drained camera lease.
 * Uses the unpublished framebuffer under the display mutex, without publishing. */
esp_err_t app_ui_test_jpeg(uint8_t *output, size_t capacity, size_t *length);
