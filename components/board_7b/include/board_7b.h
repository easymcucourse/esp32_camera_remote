#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Metadata only; connection screen worker owns actual drawing. */
void board_7b_set_maint_text(const char *text);
void board_7b_request_maint_screen(bool active);
void board_7b_set_sim(bool active);
void board_7b_set_info_level(unsigned level);

#define BOARD_LCD_WIDTH 1024
#define BOARD_LCD_HEIGHT 600

// One-time initialization. On failure, app_main aborts; no partial-init retry.
esp_err_t board_7b_init(const char *ssid, const char *password);
// Call only before starting the JPEG worker or after it has drained.
esp_err_t board_7b_show_connection(const char *status);
void board_7b_set_wifi_info(const char *ssid, const char *password, bool show_password, const char *ip, bool default_password);
void board_7b_refresh_wifi_info(void); /* Nonblocking notification to the render worker. */
// Refresh connection-screen peripheral status; Atom loss also disconnects DS4.
void board_7b_set_atom_status(bool atom_online, bool controller_online);
/* Battery is the input protocol's 0..10 level, or 255 unknown. */
void board_7b_set_controller_battery(unsigned level, bool xbox);
void board_7b_set_atom_protocol(bool mismatch, unsigned gimbal_link_state);
// Live-view status shown in the upper-right corner.
void board_7b_set_wifi_rssi(int rssi);
void board_7b_set_camera_info(const char *model, const char *firmware);
void board_7b_set_exposure_mode(uint32_t mode);
void board_7b_set_camera_property(uint16_t code, uint32_t value);
/* Status numbers: idle=0, pending=1, applied=2, rejected=3, timeout=4, accepted=5.
 * Atomic publication; rendering stays with the LCD worker. */
void board_7b_set_command_status(uint16_t code, unsigned status);
void board_7b_set_recording_status(bool known, bool recording);
bool board_7b_toggle_settings_mode(void);
bool board_7b_settings_mode(void);
void board_7b_menu_move(int direction);
unsigned board_7b_menu_selected(void);
bool board_7b_extra_menu_active(void);
void board_7b_extra_menu_open(bool active);
void board_7b_extra_menu_move(int direction);
bool board_7b_extra_menu_exit_selected(void);
/* Maintenance menu: 0 off, 1 confirm, 2 pending, 3 on, 4 failed. */
void board_7b_set_maint_menu(unsigned state);
typedef struct {
    bool active;
    unsigned selected;
    char lines[9][80], footer[96];
} board_wifi_menu_view_t;
void board_7b_set_wifi_menu(const board_wifi_menu_view_t *view);
uint32_t board_7b_connection_generation(void);
/* Seven primary parameter rows followed by nine extra parameter rows. */
void board_7b_set_menu_item(unsigned index, bool writable, unsigned status,
                           bool target_valid, uint32_t target);
typedef struct { uint32_t actual, target; unsigned status; bool writable, target_valid; } board_extra_status_t;
bool board_7b_get_extra_status(unsigned index, board_extra_status_t *out);
/* Single JPEG worker only. INVALID_RESPONSE drops this image; INVALID_STATE
 * requires recovery. No framebuffer is published on a decode error. */
esp_err_t board_7b_show_jpeg(const uint8_t *jpeg, size_t length);
/* Serialized with rendering; at most three panel recovery attempts. */
esp_err_t board_7b_recover_display(void);
/* Fatal recovery failure; app_main drains the camera and restarts on internal stack. */
bool board_7b_display_failed(void);
typedef struct { unsigned fps_tenths, battery, focus; int32_t ev; bool settings, failed, default_password; } board_status_t;
void board_7b_get_status(board_status_t *out);
esp_err_t board_7b_test_display_fault(unsigned mode);
/* Developer-only synthetic input; caller must own the drained camera lease.
 * Uses the unpublished framebuffer under the display mutex, without publishing. */
esp_err_t board_7b_test_jpeg(uint8_t *output, size_t capacity, size_t *length);
