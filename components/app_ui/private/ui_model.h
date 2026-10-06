#pragma once
#include "app_ui_internal.h"
#include "camera_settings.h"
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"

/* Component-private shared model. Existing atomic/critical-section rules are
 * preserved; the renderer remains the only pixel writer. */
typedef struct {
    bool writable, target_valid;
    unsigned status;
    uint32_t target, status_ms;
} menu_view_t;
extern atomic_bool ui_model_display_failed;
extern atomic_uint ui_model_fps_tenths;
extern char ui_model_connection_ssid[33], ui_model_connection_password[65];
extern bool ui_model_connection_default_password;
extern atomic_uint ui_model_info_level;
extern char ui_model_connection_ip[16];
extern portMUX_TYPE ui_model_wifi_info_mux;
extern atomic_bool ui_model_wifi_info_dirty;
extern char ui_model_camera_model[24];
extern char ui_model_camera_firmware[24];
extern atomic_int ui_model_wifi_rssi;
extern atomic_uint ui_model_exposure_mode;
extern atomic_uint ui_model_mode_command_status, ui_model_focus_command_status;
extern atomic_uint ui_model_action_command_status;
extern atomic_uint ui_model_mode_status_ms, ui_model_focus_status_ms, ui_model_action_status_ms;
extern atomic_uint ui_model_recording_state, ui_model_recording_started_s;
extern atomic_uint ui_model_camera_battery;
extern atomic_uint ui_model_controller_battery;
extern atomic_bool ui_model_settings_mode;
extern atomic_bool ui_model_sim_active;
extern atomic_uint ui_model_menu_selected;
extern atomic_uint ui_model_connection_generation;
extern menu_view_t ui_model_menu_view[7 + CAMERA_EXTRA_COUNT];
extern atomic_bool ui_model_extra_menu_active;
extern atomic_uint ui_model_extra_menu_selected;
extern portMUX_TYPE ui_model_menu_mux;
extern atomic_uint ui_model_prop_iso;
extern atomic_uint ui_model_prop_shutter;
extern atomic_uint ui_model_prop_aperture;
extern atomic_int ui_model_prop_ev;
extern atomic_uint ui_model_prop_wb;
extern atomic_uint ui_model_prop_focus;
extern atomic_uint ui_model_prop_meter;
extern atomic_uint ui_model_prop_flash;
extern atomic_uint ui_model_prop_extra[CAMERA_EXTRA_COUNT];
extern atomic_bool ui_model_atom_connected, ui_model_controller_connected;
extern atomic_bool ui_model_atom_protocol_mismatch;
extern atomic_uint ui_model_gimbal_link_state;

/* Core fixed-display transition only, after all normal writers/readers drain. */
extern atomic_bool ui_model_frozen;
void ui_model_freeze_and_clear(void);
