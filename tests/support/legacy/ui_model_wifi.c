#include "ui_camera_vendor_codes.h"
#include "ui_model.h"
#include "ui_overlay.h"
#include "camera_menu_navigation.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>

atomic_bool ui_model_display_failed;
atomic_uint ui_model_fps_tenths;
char ui_model_connection_ssid[33], ui_model_connection_password[65];
bool ui_model_connection_default_password;
atomic_uint ui_model_info_level;
char ui_model_connection_ip[16] = "--";
portMUX_TYPE ui_model_wifi_info_mux = portMUX_INITIALIZER_UNLOCKED;
char ui_model_maint_text[128];
atomic_bool ui_model_wifi_info_dirty;
atomic_uint ui_model_maint_screen_requested;
char ui_model_camera_model[24] = "UNKNOWN";
char ui_model_camera_firmware[24] = "UNKNOWN";
atomic_int ui_model_wifi_rssi = -127;
atomic_uint ui_model_exposure_mode = 0xffffffff;
atomic_uint ui_model_mode_command_status, ui_model_focus_command_status;
atomic_uint ui_model_action_command_status;
atomic_uint ui_model_mode_status_ms, ui_model_focus_status_ms, ui_model_action_status_ms;
atomic_uint ui_model_recording_state, ui_model_recording_started_s;
atomic_uint ui_model_camera_battery = 255;
atomic_uint ui_model_controller_battery = 255;
atomic_bool ui_model_settings_mode;
atomic_bool ui_model_sim_active;
atomic_uint ui_model_menu_selected = 5;
atomic_uint ui_model_maint_menu_state;
atomic_uint ui_model_connection_generation;
app_ui_wifi_menu_view_t ui_model_wifi_menu_view;
portMUX_TYPE ui_model_wifi_menu_mux = portMUX_INITIALIZER_UNLOCKED;
menu_view_t ui_model_menu_view[7 + CAMERA_EXTRA_COUNT];
atomic_bool ui_model_extra_menu_active;
atomic_uint ui_model_extra_menu_selected;
portMUX_TYPE ui_model_menu_mux = portMUX_INITIALIZER_UNLOCKED;
atomic_uint ui_model_prop_iso = 0xffffffff;
atomic_uint ui_model_prop_shutter = 0xffffffff;
atomic_uint ui_model_prop_aperture = 0xffff;
atomic_int ui_model_prop_ev = INT32_MIN;
atomic_uint ui_model_prop_wb = 0xffff;
atomic_uint ui_model_prop_focus = 0xffff;
atomic_uint ui_model_prop_meter = 0xffff;
atomic_uint ui_model_prop_flash = 0xffff;
atomic_uint ui_model_prop_extra[CAMERA_EXTRA_COUNT] = {
    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX,
    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
};
atomic_bool ui_model_atom_connected, ui_model_controller_connected;
atomic_bool ui_model_atom_protocol_mismatch;
atomic_uint ui_model_gimbal_link_state;

void app_ui_set_info_level(unsigned level) { if (level<=UI_INFO_HIDDEN) atomic_store(&ui_model_info_level,level); }

void app_ui_request_maint_screen(bool active)
{
    atomic_store(&ui_model_maint_screen_requested,active?1:2);atomic_store(&ui_model_wifi_info_dirty,true);
    app_ui_refresh_wifi_info();
}

void app_ui_set_maint_text(const char *text)
{
    portENTER_CRITICAL(&ui_model_wifi_info_mux);
    bool changed=strcmp(ui_model_maint_text,text)!=0;
    if (changed) snprintf(ui_model_maint_text,sizeof(ui_model_maint_text),"%s",text);
    portEXIT_CRITICAL(&ui_model_wifi_info_mux);
    if (changed) { atomic_store(&ui_model_wifi_info_dirty,true);app_ui_refresh_wifi_info(); }
}

void app_ui_set_maint_menu(unsigned state)
{
    if (atomic_exchange(&ui_model_maint_menu_state,state)!=state) atomic_store(&ui_model_wifi_info_dirty,true);
}

void app_ui_get_status(app_ui_status_t *out)
{
    *out = (app_ui_status_t){.fps_tenths = atomic_load(&ui_model_fps_tenths),
        .battery = atomic_load(&ui_model_camera_battery), .focus = atomic_load(&ui_model_prop_focus), .ev = atomic_load(&ui_model_prop_ev),
        .settings = atomic_load(&ui_model_settings_mode), .failed = atomic_load(&ui_model_display_failed)};
    portENTER_CRITICAL(&ui_model_wifi_info_mux);out->default_password=ui_model_connection_default_password;portEXIT_CRITICAL(&ui_model_wifi_info_mux);
}

void app_ui_set_wifi_info(const char *ssid, const char *password, bool show_password, const char *ip, bool default_password)
{
    char new_ssid[33] = {0}, new_password[65] = {0}, new_ip[16] = {0};
    snprintf(new_ssid, sizeof(new_ssid), "%s", ssid ? ssid : "");
    snprintf(new_password, sizeof(new_password), "%s", show_password && password ? password : "********");
    snprintf(new_ip, sizeof(new_ip), "%s", ip && *ip ? ip : "--");
    portENTER_CRITICAL(&ui_model_wifi_info_mux);
    bool changed = strcmp(ui_model_connection_ssid, new_ssid) || strcmp(ui_model_connection_password, new_password) || strcmp(ui_model_connection_ip, new_ip) || ui_model_connection_default_password!=default_password;
    if (changed) {
        memcpy(ui_model_connection_ssid, new_ssid, sizeof(new_ssid));
        memcpy(ui_model_connection_password, new_password, sizeof(new_password));
        memcpy(ui_model_connection_ip, new_ip, sizeof(new_ip));
        ui_model_connection_default_password=default_password;
        atomic_store(&ui_model_wifi_info_dirty, true);
    }
    portEXIT_CRITICAL(&ui_model_wifi_info_mux);
}

void app_ui_set_sim(bool active)
{
    if (atomic_exchange(&ui_model_sim_active,active)==active) return;
    atomic_store(&ui_model_wifi_info_dirty,true);
    app_ui_refresh_wifi_info();
}

void app_ui_set_atom_status(bool atom_online, bool controller_online)
{
    controller_online = (atom_online || atomic_load(&ui_model_sim_active)) && controller_online;
    if (!controller_online) atomic_fetch_or(&ui_model_controller_battery, 255);
    // Steady-state polling must not wait behind JPEG decoding/publication.
    if (atomic_load(&ui_model_atom_connected) == atom_online &&
        atomic_load(&ui_model_controller_connected) == controller_online) return;
    atomic_store(&ui_model_atom_connected, atom_online);
    atomic_store(&ui_model_controller_connected, controller_online);
    atomic_store(&ui_model_wifi_info_dirty, true);
    app_ui_refresh_wifi_info();
}

void app_ui_set_controller_battery(unsigned level, bool xbox)
{
    atomic_store(&ui_model_controller_battery, (xbox ? 256u : 0u) | (level <= 10 ? level * 10 : 255u));
}

void app_ui_set_atom_protocol(bool mismatch, unsigned gimbal)
{
    if (gimbal > 3) gimbal = 0;
    if (atomic_load(&ui_model_atom_protocol_mismatch) == mismatch && atomic_load(&ui_model_gimbal_link_state) == gimbal) return;
    atomic_store(&ui_model_atom_protocol_mismatch, mismatch); atomic_store(&ui_model_gimbal_link_state, gimbal);
    atomic_store(&ui_model_wifi_info_dirty, true);
    app_ui_refresh_wifi_info();
}

void app_ui_set_wifi_rssi(int rssi)
{
    atomic_store(&ui_model_wifi_rssi, rssi);
}

void app_ui_set_camera_info(const char *model, const char *firmware)
{
    if (model && *model) snprintf(ui_model_camera_model, sizeof(ui_model_camera_model), "%s", model);
    if (firmware && *firmware) snprintf(ui_model_camera_firmware, sizeof(ui_model_camera_firmware), "%s", firmware);
}

void app_ui_set_exposure_mode(uint32_t mode)
{
    atomic_store(&ui_model_exposure_mode, mode);
}

void app_ui_set_camera_property(uint16_t code, uint32_t value)
{
    switch (code) {
    case 0x5005: atomic_store(&ui_model_prop_wb, value); break;
    case 0x5007: atomic_store(&ui_model_prop_aperture, value); break;
    case 0x500a: atomic_store(&ui_model_prop_focus, value); break;
    case 0x500b: atomic_store(&ui_model_prop_meter, value); break;
    case 0x500c: atomic_store(&ui_model_prop_flash, value); break;
    case 0x5010: atomic_store(&ui_model_prop_ev, (int16_t)value); break;
    case 0xd20d: atomic_store(&ui_model_prop_shutter, value); break;
    case 0xd21e: atomic_store(&ui_model_prop_iso, value); break;
    case 0xd218: atomic_store(&ui_model_camera_battery, value <= 100 ? value : 255); break;
    default:
        for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i)
            if (code == camera_extra_codes[i]) atomic_store(&ui_model_prop_extra[i], value);
        break;
    }
}

bool app_ui_settings_mode(void) { return atomic_load(&ui_model_settings_mode); }

unsigned app_ui_menu_selected(void) { return atomic_load(&ui_model_extra_menu_active) ? 7 + atomic_load(&ui_model_extra_menu_selected) : atomic_load(&ui_model_menu_selected); }

bool app_ui_extra_menu_active(void) { return atomic_load(&ui_model_extra_menu_active); }

bool app_ui_extra_menu_exit_selected(void) { return atomic_load(&ui_model_extra_menu_selected) == CAMERA_EXTRA_COUNT; }

void app_ui_extra_menu_open(bool active)
{
    if (active) atomic_store(&ui_model_extra_menu_selected, 0);
    atomic_store(&ui_model_extra_menu_active, active);
    atomic_store(&ui_model_wifi_info_dirty, true);
}

void app_ui_extra_menu_move(int direction)
{
    if (direction != 1 && direction != -1) return;
    unsigned previous = atomic_load(&ui_model_extra_menu_selected), next;
    do { next = camera_menu_extra_next(previous, direction, CAMERA_EXTRA_COUNT); }
    while (!atomic_compare_exchange_weak(&ui_model_extra_menu_selected, &previous, next));
    atomic_store(&ui_model_wifi_info_dirty, true);
}

uint32_t app_ui_connection_generation(void) { return atomic_load(&ui_model_connection_generation); }

void app_ui_set_wifi_menu(const app_ui_wifi_menu_view_t *view)
{
    portENTER_CRITICAL(&ui_model_wifi_menu_mux);
    bool changed = memcmp(&ui_model_wifi_menu_view, view, sizeof(*view)) != 0;
    if (changed) ui_model_wifi_menu_view = *view;
    portEXIT_CRITICAL(&ui_model_wifi_menu_mux);
    if (changed) atomic_store(&ui_model_wifi_info_dirty, true);
}

void app_ui_menu_move(int direction)
{
    if (!atomic_load(&ui_model_settings_mode) || (direction != -1 && direction != 1)) return;
    unsigned previous = atomic_load(&ui_model_menu_selected), next;
    /* Menu IDs stay tied to camera properties; navigation follows screen order. */
    do { next = camera_menu_main_next(previous, direction); }
    while (!atomic_compare_exchange_weak(&ui_model_menu_selected, &previous, next));
    atomic_store(&ui_model_wifi_info_dirty, true);
}

void app_ui_set_menu_item(unsigned index, bool writable, unsigned status,
                           bool target_valid, uint32_t target)
{
    if (index >= 7 + CAMERA_EXTRA_COUNT || status > 5) return;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&ui_model_menu_mux);
    menu_view_t *v = &ui_model_menu_view[index];
    if (v->status != status || v->target != target || v->target_valid != target_valid) v->status_ms = now;
    v->writable = writable; v->status = status;
    v->target_valid = target_valid; v->target = target;
    portEXIT_CRITICAL(&ui_model_menu_mux);
}

bool app_ui_get_extra_status(unsigned index, app_ui_extra_status_t *out)
{
    if (index >= CAMERA_EXTRA_COUNT || !out) return false;
    portENTER_CRITICAL(&ui_model_menu_mux);
    menu_view_t v = ui_model_menu_view[7 + index];
    portEXIT_CRITICAL(&ui_model_menu_mux);
    *out = (app_ui_extra_status_t){.actual = atomic_load(&ui_model_prop_extra[index]), .target = v.target,
        .status = v.status, .writable = v.writable, .target_valid = v.target_valid};
    return true;
}

bool app_ui_toggle_settings_mode(void)
{
    bool previous = atomic_load(&ui_model_settings_mode);
    while (!atomic_compare_exchange_weak(&ui_model_settings_mode, &previous, !previous)) {}
    atomic_store(&ui_model_wifi_info_dirty, true);
    if (previous) app_ui_extra_menu_open(false);
    return !previous;
}

void app_ui_set_command_status(uint16_t code, unsigned status)
{
    if (status > 5) return;
    atomic_uint *value = NULL, *changed = NULL;
    if (code == 0x500e) { value = &ui_model_mode_command_status; changed = &ui_model_mode_status_ms; }
    if (code == 0x500a) { value = &ui_model_focus_command_status; changed = &ui_model_focus_status_ms; }
    if (code == 0xd2c1 || code == 0xd2c2 || code == 0xd2c8 || code == 0xd2dd) {
        value = &ui_model_action_command_status; changed = &ui_model_action_status_ms;
    }
    if (value && atomic_exchange(value, status) != status)
        atomic_store(changed, (unsigned)(esp_timer_get_time() / 1000));
}

void app_ui_set_recording_status(bool known, bool recording)
{
    unsigned state = !known ? (atomic_load(&ui_model_recording_state) == 2 ? 2 : 0) : recording ? 2 : 1;
    unsigned previous = atomic_exchange(&ui_model_recording_state, state);
    if (state == 2 && previous != 2)
        atomic_store(&ui_model_recording_started_s, (unsigned)(esp_timer_get_time() / 1000000));
}
