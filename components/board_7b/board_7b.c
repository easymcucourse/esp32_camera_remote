#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "board_7b.h"
#include "ui_overlay.h"
#include "image_stride.h"
#include "board_lcd.h"
#include "camera_settings.h"
#include "camera_menu_navigation.h"
#include "ui_fonts.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "rom/tjpgd.h"
#include "esp_jpeg_dec.h"
#if CONFIG_REMOTE_DBG_SIM
#include "esp_jpeg_enc.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"

static const char *TAG = "board_7b";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expander;
// Used only by the JPEG worker; reuse buffers across frames.
static void *jpeg_work;
static uint16_t *jpeg_pixels;
static unsigned last_width, last_height;
static atomic_bool display_failed;
static jpeg_dec_handle_t fast_decoder;
static int64_t fps_window_start, fps_last_frame;
static unsigned fps_intervals;
static atomic_uint fps_tenths;
static char connection_ssid[33], connection_password[65];
static bool connection_default_password;
static atomic_uint info_level;
void board_7b_set_info_level(unsigned level) { if (level<=UI_INFO_HIDDEN) atomic_store(&info_level,level); }
static char connection_ip[16] = "--";
static portMUX_TYPE wifi_info_mux = portMUX_INITIALIZER_UNLOCKED;
static char maint_text[128];
static atomic_bool wifi_info_dirty;
static atomic_uint maint_screen_requested;
void board_7b_request_maint_screen(bool active)
{
    atomic_store(&maint_screen_requested,active?1:2);atomic_store(&wifi_info_dirty,true);
    board_7b_refresh_wifi_info();
}
void board_7b_set_maint_text(const char *text)
{
    portENTER_CRITICAL(&wifi_info_mux);
    bool changed=strcmp(maint_text,text)!=0;
    if (changed) snprintf(maint_text,sizeof(maint_text),"%s",text);
    portEXIT_CRITICAL(&wifi_info_mux);
    if (changed) { atomic_store(&wifi_info_dirty,true);board_7b_refresh_wifi_info(); }
}
static char connection_status_text[96] = "Starting Wi-Fi hotspot...";
static char camera_model[24] = "UNKNOWN";
static char camera_firmware[24] = "UNKNOWN";
static atomic_int wifi_rssi = -127;
static atomic_uint exposure_mode = 0xffffffff;
static atomic_uint mode_command_status, focus_command_status;
static atomic_uint action_command_status;
static atomic_uint mode_status_ms, focus_status_ms, action_status_ms;
static atomic_uint recording_state, recording_started_s;
static atomic_uint camera_battery = 255;
static atomic_uint controller_battery = 255; /* Percent in low byte; Xbox flag in bit 8. */
static atomic_bool settings_mode;
static atomic_bool sim_active;
static atomic_uint menu_selected = 5; /* Focus is the first editable screen row. */
static atomic_uint maint_menu_state;
void board_7b_set_maint_menu(unsigned state)
{
    if (atomic_exchange(&maint_menu_state,state)!=state) atomic_store(&wifi_info_dirty,true);
}
static atomic_uint connection_generation;
static board_wifi_menu_view_t wifi_menu_view;
static portMUX_TYPE wifi_menu_mux = portMUX_INITIALIZER_UNLOCKED;
static void draw_settings_panel(uint16_t *pixels);
typedef struct {
    bool writable, target_valid;
    unsigned status;
    uint32_t target, status_ms;
} menu_view_t;
static menu_view_t menu_view[7 + CAMERA_EXTRA_COUNT];
static atomic_bool extra_menu_active;
static atomic_uint extra_menu_selected;
static portMUX_TYPE menu_mux = portMUX_INITIALIZER_UNLOCKED;
static atomic_uint prop_iso = 0xffffffff;
static atomic_uint prop_shutter = 0xffffffff;
static atomic_uint prop_aperture = 0xffff;
static atomic_int prop_ev = INT32_MIN;
static atomic_uint prop_wb = 0xffff;
static atomic_uint prop_focus = 0xffff;
static atomic_uint prop_meter = 0xffff;
static atomic_uint prop_flash = 0xffff;
static atomic_uint prop_extra[CAMERA_EXTRA_COUNT] = {
    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX,
    UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX
};
static SemaphoreHandle_t display_mutex;
static TaskHandle_t refresh_task;
static bool showing_connection = true;
static atomic_bool atom_connected, controller_connected;
static atomic_bool atom_protocol_mismatch;
static atomic_uint gimbal_link_state;

static void draw_text(uint16_t *pixels, int left, int top, const char *text,
                      int pixel_size, uint16_t color)
{
    ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, left, top, text,
                  pixel_size, color, false, BOARD_LCD_WIDTH);
}

static int fit_font_size(const char *text, int requested, int max_width, bool numbers)
{
    while (requested > 12 && ui_fonts_measure(text, requested, numbers) > max_width)
        --requested;
    return requested;
}

static void draw_connection(uint16_t *pixels, const char *status)
{
    for (int i = 0; i < BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT; ++i) pixels[i] = 0x1082;
    const char *title = "easymcucourse camera console";
    draw_text(pixels, 48, 40, title,
              fit_font_size(title, 40, BOARD_LCD_WIDTH - 96, false), 0xffff);
    if (atomic_load(&sim_active)) draw_text(pixels, 48, 92, "SIM", 24, 0xffe0);
    char line[96];
    char ssid[33], password[65], ip[16];
    portENTER_CRITICAL(&wifi_info_mux);
    memcpy(ssid, connection_ssid, sizeof(ssid));
    memcpy(password, connection_password, sizeof(password));
    memcpy(ip, connection_ip, sizeof(ip));
    bool default_password=connection_default_password;
    portEXIT_CRITICAL(&wifi_info_mux);
    snprintf(line, sizeof(line), "SSID: %s", ssid);
    draw_text(pixels, 48, 130, line,
              fit_font_size(line, 32, BOARD_LCD_WIDTH - 96, false), 0xffff);
    snprintf(line, sizeof(line), "Password: %s", password);
    int password_font=fit_font_size(line,32,BOARD_LCD_WIDTH-96-(default_password?160:0),false);
    draw_text(pixels,48,190,line,password_font,0xffff);
    if (default_password) draw_text(pixels,48+ui_fonts_measure(line,password_font,false)+12,190,"DEFAULT",24,0xffe0);
    snprintf(line, sizeof(line), "IP: %s", ip);
    draw_text(pixels, 48, 232, line, 22, 0x7bef);
    snprintf(line, sizeof(line), "Expansion unit (ATOM): %s",
             atom_protocol_mismatch ? "Firmware version mismatch" : atom_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 270, line, 30, atom_connected ? 0x07e0 : 0xf800);
    snprintf(line, sizeof(line), "Controller (DS4): %s",
             controller_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 330, line, 30, controller_connected ? 0x07e0 : 0xf800);
    static const char *const gimbal_states[] = {"Disabled / disconnected", "Searching", "Connecting", "Connected"};
    unsigned gimbal = atomic_load(&gimbal_link_state);
    snprintf(line, sizeof(line), "Gimbal: %s", gimbal_states[gimbal < 4 ? gimbal : 0]);
    draw_text(pixels, 48, 370, line, 22, gimbal == 3 ? 0x07e0 : 0x7bef);
    draw_text(pixels, 48, 410, status,
              fit_font_size(status, 24, BOARD_LCD_WIDTH - 96, false), 0x07ff);
    draw_text(pixels, 48, 466, "Connect camera to this Wi-Fi.", 24, 0x7bef);
    draw_text(pixels, 48, 516, "Enable PC Remote on camera.", 24, 0x7bef);
    char maintenance[128];
    portENTER_CRITICAL(&wifi_info_mux);memcpy(maintenance,maint_text,sizeof(maintenance));portEXIT_CRITICAL(&wifi_info_mux);
    if (*maintenance) draw_text(pixels,48,560,maintenance,fit_font_size(maintenance,18,BOARD_LCD_WIDTH-96,false),0xffe0);
    if (atomic_load(&settings_mode)) draw_settings_panel(pixels);
}

static esp_err_t write_register(uint8_t reg, uint8_t value);

static void prepare_connection(uint16_t *pixels)
{
    draw_connection(pixels, connection_status_text);
}

/* Caller holds display_mutex. Decoder state never outlives a panel recovery. */
static esp_err_t recover_display(void)
{
    if (atomic_load(&display_failed)) return ESP_FAIL;
    if (board_lcd_ready()) return ESP_OK;
    ESP_LOGW(TAG, "Recovering display: stopping RGB, replacing buffers and JPEG decoders");
    const uint8_t outputs = 0xff & ~(1 << 2) & ~(1 << 5);
    esp_err_t backlight = write_register(0x03, outputs);
    if (backlight != ESP_OK) ESP_LOGW(TAG, "Recovery backlight off: %s", esp_err_to_name(backlight));
    if (fast_decoder) jpeg_dec_close(fast_decoder);
    fast_decoder = NULL;
    heap_caps_free(jpeg_work); jpeg_work = NULL; jpeg_pixels = NULL;
    last_width = last_height = 0;
    fps_last_frame = fps_window_start = 0;
    fps_intervals = fps_tenths = 0;
    if (!showing_connection) atomic_fetch_add(&connection_generation, 1);
    showing_connection = true;
    esp_err_t err = board_lcd_recover(prepare_connection);
    if (err == ESP_OK) {
        backlight = write_register(0x03, outputs | (1 << 2));
        if (backlight != ESP_OK) ESP_LOGW(TAG, "Recovery backlight on: %s", esp_err_to_name(backlight));
    } else {
        atomic_store(&display_failed, true);
        ESP_LOGE(TAG, "LCD recovery exhausted: %s; requesting controlled restart", esp_err_to_name(err));
    }
    return err;
}

esp_err_t board_7b_recover_display(void)
{
    if (!display_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    esp_err_t err = recover_display();
    xSemaphoreGive(display_mutex);
    return err;
}

bool board_7b_display_failed(void) { return atomic_load(&display_failed); }
void board_7b_get_status(board_status_t *out)
{
    *out = (board_status_t){.fps_tenths = atomic_load(&fps_tenths),
        .battery = atomic_load(&camera_battery), .focus = atomic_load(&prop_focus), .ev = atomic_load(&prop_ev),
        .settings = atomic_load(&settings_mode), .failed = atomic_load(&display_failed)};
    portENTER_CRITICAL(&wifi_info_mux);out->default_password=connection_default_password;portEXIT_CRITICAL(&wifi_info_mux);
}
esp_err_t board_7b_test_display_fault(unsigned mode)
{
    esp_err_t err = board_lcd_test_fault(mode);
    if (err == ESP_OK) { atomic_store(&wifi_info_dirty, true); board_7b_refresh_wifi_info(); }
    return err;
}

static esp_err_t publish_frame(uint16_t *pixels)
{
    return board_lcd_publish(pixels) == ESP_OK ? ESP_OK : ESP_ERR_INVALID_STATE;
}
void board_7b_set_wifi_info(const char *ssid, const char *password, bool show_password, const char *ip, bool default_password)
{
    char new_ssid[33] = {0}, new_password[65] = {0}, new_ip[16] = {0};
    snprintf(new_ssid, sizeof(new_ssid), "%s", ssid ? ssid : "");
    snprintf(new_password, sizeof(new_password), "%s", show_password && password ? password : "********");
    snprintf(new_ip, sizeof(new_ip), "%s", ip && *ip ? ip : "--");
    portENTER_CRITICAL(&wifi_info_mux);
    bool changed = strcmp(connection_ssid, new_ssid) || strcmp(connection_password, new_password) || strcmp(connection_ip, new_ip) || connection_default_password!=default_password;
    if (changed) {
        memcpy(connection_ssid, new_ssid, sizeof(new_ssid));
        memcpy(connection_password, new_password, sizeof(new_password));
        memcpy(connection_ip, new_ip, sizeof(new_ip));
        connection_default_password=default_password;
        atomic_store(&wifi_info_dirty, true);
    }
    portEXIT_CRITICAL(&wifi_info_mux);
}
void board_7b_refresh_wifi_info(void)
{
    /* Wi-Fi/NVS and input workers never render or wait behind a stalled LCD. */
    if (refresh_task) xTaskNotifyGive(refresh_task);
}

static void connection_refresh_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
        if (!atomic_load(&wifi_info_dirty) || atomic_load(&display_failed)) continue;
        if (xSemaphoreTake(display_mutex, 0) != pdTRUE) continue;
        atomic_store(&wifi_info_dirty, false);
        unsigned maintenance_request=atomic_exchange(&maint_screen_requested,0);
        if (maintenance_request==1) {
            if (!showing_connection) atomic_fetch_add(&connection_generation,1);
            showing_connection=true;atomic_store(&settings_mode,false);
        }
        if (maintenance_request && showing_connection) {
            snprintf(connection_status_text,sizeof(connection_status_text),"%s",maintenance_request==1?"Maintenance mode":"Maintenance off - waiting for camera...");
        }
        if (showing_connection) {
            esp_err_t err = recover_display();
            if (err == ESP_OK) {
                uint16_t *pixels = board_lcd_back_buffer();
                draw_connection(pixels, connection_status_text);
                err = publish_frame(pixels);
                if (err != ESP_OK) recover_display();
            }
        }
        xSemaphoreGive(display_mutex);
    }
}

esp_err_t board_7b_show_connection(const char *status)
{
    if (!display_mutex || !status) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    snprintf(connection_status_text, sizeof(connection_status_text), "%s", status);
    if (!showing_connection) atomic_fetch_add(&connection_generation, 1);
    showing_connection = true;
    esp_err_t err = recover_display();
    if (err != ESP_OK) { xSemaphoreGive(display_mutex); return err; }
    uint16_t *pixels = board_lcd_back_buffer();
    draw_connection(pixels, connection_status_text);
    fps_last_frame = fps_window_start = 0;
    fps_intervals = fps_tenths = 0;
    err = publish_frame(pixels);
    if (err != ESP_OK) err = recover_display();
    xSemaphoreGive(display_mutex);
    return err;
}

void board_7b_set_sim(bool active)
{
    if (atomic_exchange(&sim_active,active)==active) return;
    atomic_store(&wifi_info_dirty,true);
    board_7b_refresh_wifi_info();
}
void board_7b_set_atom_status(bool atom_online, bool controller_online)
{
    controller_online = atom_online && controller_online;
    if (!controller_online) atomic_fetch_or(&controller_battery, 255);
    // Steady-state polling must not wait behind JPEG decoding/publication.
    if (atomic_load(&atom_connected) == atom_online &&
        atomic_load(&controller_connected) == controller_online) return;
    atomic_store(&atom_connected, atom_online);
    atomic_store(&controller_connected, controller_online);
    atomic_store(&wifi_info_dirty, true);
    board_7b_refresh_wifi_info();
}

void board_7b_set_controller_battery(unsigned level, bool xbox)
{
    atomic_store(&controller_battery, (xbox ? 256u : 0u) | (level <= 10 ? level * 10 : 255u));
}

static unsigned format_controller_battery(char *text, size_t size)
{
    unsigned state = atomic_load(&controller_battery);
    unsigned percent = atomic_load(&controller_connected) ? state & 255u : 255u;
    const char *name = state & 256u ? "XBOX" : "DS4";
    if (percent > 100) snprintf(text, size, "%s: --", name);
    else snprintf(text, size, "%s: %u%%", name, percent);
    return percent;
}

static uint16_t battery_color(unsigned percent)
{
    if (percent > 100) return 0x7bef;
    if (percent <= 20) return 0xf800;
    if (percent <= 50) return 0xffe0;
    return 0x07e0;
}

void board_7b_set_atom_protocol(bool mismatch, unsigned gimbal)
{
    if (gimbal > 3) gimbal = 0;
    if (atomic_load(&atom_protocol_mismatch) == mismatch && atomic_load(&gimbal_link_state) == gimbal) return;
    atomic_store(&atom_protocol_mismatch, mismatch); atomic_store(&gimbal_link_state, gimbal);
    atomic_store(&wifi_info_dirty, true);
    board_7b_refresh_wifi_info();
}

void board_7b_set_wifi_rssi(int rssi)
{
    atomic_store(&wifi_rssi, rssi);
}

void board_7b_set_camera_info(const char *model, const char *firmware)
{
    if (model && *model) snprintf(camera_model, sizeof(camera_model), "%s", model);
    if (firmware && *firmware) snprintf(camera_firmware, sizeof(camera_firmware), "%s", firmware);
}

void board_7b_set_exposure_mode(uint32_t mode)
{
    atomic_store(&exposure_mode, mode);
}

void board_7b_set_camera_property(uint16_t code, uint32_t value)
{
    switch (code) {
    case 0x5005: atomic_store(&prop_wb, value); break;
    case 0x5007: atomic_store(&prop_aperture, value); break;
    case 0x500a: atomic_store(&prop_focus, value); break;
    case 0x500b: atomic_store(&prop_meter, value); break;
    case 0x500c: atomic_store(&prop_flash, value); break;
    case 0x5010: atomic_store(&prop_ev, (int16_t)value); break;
    case 0xd20d: atomic_store(&prop_shutter, value); break;
    case 0xd21e: atomic_store(&prop_iso, value); break;
    case 0xd218: atomic_store(&camera_battery, value <= 100 ? value : 255); break;
    default:
        for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i)
            if (code == camera_extra_codes[i]) atomic_store(&prop_extra[i], value);
        break;
    }
}

bool board_7b_settings_mode(void) { return atomic_load(&settings_mode); }
unsigned board_7b_menu_selected(void) { return atomic_load(&extra_menu_active) ? 7 + atomic_load(&extra_menu_selected) : atomic_load(&menu_selected); }
bool board_7b_extra_menu_active(void) { return atomic_load(&extra_menu_active); }
bool board_7b_extra_menu_exit_selected(void) { return atomic_load(&extra_menu_selected) == CAMERA_EXTRA_COUNT; }
void board_7b_extra_menu_open(bool active)
{
    if (active) atomic_store(&extra_menu_selected, 0);
    atomic_store(&extra_menu_active, active);
    atomic_store(&wifi_info_dirty, true);
}
void board_7b_extra_menu_move(int direction)
{
    if (direction != 1 && direction != -1) return;
    unsigned previous = atomic_load(&extra_menu_selected), next;
    do { next = camera_menu_extra_next(previous, direction, CAMERA_EXTRA_COUNT); }
    while (!atomic_compare_exchange_weak(&extra_menu_selected, &previous, next));
    atomic_store(&wifi_info_dirty, true);
}
uint32_t board_7b_connection_generation(void) { return atomic_load(&connection_generation); }
void board_7b_set_wifi_menu(const board_wifi_menu_view_t *view)
{
    portENTER_CRITICAL(&wifi_menu_mux);
    bool changed = memcmp(&wifi_menu_view, view, sizeof(*view)) != 0;
    if (changed) wifi_menu_view = *view;
    portEXIT_CRITICAL(&wifi_menu_mux);
    if (changed) atomic_store(&wifi_info_dirty, true);
}
void board_7b_menu_move(int direction)
{
    if (!atomic_load(&settings_mode) || (direction != -1 && direction != 1)) return;
    unsigned previous = atomic_load(&menu_selected), next;
    /* Menu IDs stay tied to camera properties; navigation follows screen order. */
    do { next = camera_menu_main_next(previous, direction); }
    while (!atomic_compare_exchange_weak(&menu_selected, &previous, next));
    atomic_store(&wifi_info_dirty, true);
}
void board_7b_set_menu_item(unsigned index, bool writable, unsigned status,
                           bool target_valid, uint32_t target)
{
    if (index >= 7 + CAMERA_EXTRA_COUNT || status > 5) return;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    portENTER_CRITICAL(&menu_mux);
    menu_view_t *v = &menu_view[index];
    if (v->status != status || v->target != target || v->target_valid != target_valid) v->status_ms = now;
    v->writable = writable; v->status = status;
    v->target_valid = target_valid; v->target = target;
    portEXIT_CRITICAL(&menu_mux);
}
bool board_7b_get_extra_status(unsigned index, board_extra_status_t *out)
{
    if (index >= CAMERA_EXTRA_COUNT || !out) return false;
    portENTER_CRITICAL(&menu_mux);
    menu_view_t v = menu_view[7 + index];
    portEXIT_CRITICAL(&menu_mux);
    *out = (board_extra_status_t){.actual = atomic_load(&prop_extra[index]), .target = v.target,
        .status = v.status, .writable = v.writable, .target_valid = v.target_valid};
    return true;
}

bool board_7b_toggle_settings_mode(void)
{
    bool previous = atomic_load(&settings_mode);
    while (!atomic_compare_exchange_weak(&settings_mode, &previous, !previous)) {}
    atomic_store(&wifi_info_dirty, true);
    if (previous) board_7b_extra_menu_open(false);
    return !previous;
}

static const char *exposure_mode_name(unsigned mode)
{
    switch (mode) {
    case 0x00000001: return "M";
    case 0x00010002: return "P";
    case 0x00020003: return "A";
    case 0x00030004: return "S";
    case 0x00000005: return "CREATIVE";
    case 0x00000006: return "ACTION";
    case 0x00000007: return "PORTRAIT";
    case 0x00048000: return "AUTO";
    case 0x00048001: return "AUTO+";
    case 0x00058011: return "SPORTS";
    case 0x00058012: return "SUNSET";
    case 0x00058013: return "NIGHT";
    case 0x00058014: return "LANDSCAPE";
    case 0x00058015: return "MACRO";
    case 0x00058016: return "TWILIGHT";
    case 0x00058017: return "NIGHT PORTRAIT";
    case 0x00058018: return "ANTI BLUR";
    case 0x00008020: return "MEMORY RECALL";
    case 0x00068040: return "3D PANORAMA";
    case 0x00068041: return "PANORAMA";
    case 0x00078050: return "MOVIE P";
    case 0x00078051: return "MOVIE A";
    case 0x00078052: return "MOVIE S";
    case 0x00078053: return "MOVIE M";
    case 0x00078054: return "MOVIE AUTO";
    case 0x00098059: return "S&Q P";
    case 0x0009805a: return "S&Q A";
    case 0x0009805b: return "S&Q S";
    case 0x0009805c: return "S&Q M";
    case 0x0009805d: return "S&Q AUTO";
    case 0x00088080: return "HFR P";
    case 0x00088081: return "HFR A";
    case 0x00088082: return "HFR S";
    case 0x00088083: return "HFR M";
    default: return NULL;
    }
}

static void format_exposure_mode(char *text, size_t size, unsigned mode)
{
    const char *name = exposure_mode_name(mode);
    if (mode == 0xffffffff) snprintf(text, size, "MODE --");
    else if (name) snprintf(text, size, "MODE %s", name);
    else snprintf(text, size, "MODE 0X%08X", mode);
}

void board_7b_set_command_status(uint16_t code, unsigned status)
{
    if (status > 5) return;
    atomic_uint *value = NULL, *changed = NULL;
    if (code == 0x500e) { value = &mode_command_status; changed = &mode_status_ms; }
    if (code == 0x500a) { value = &focus_command_status; changed = &focus_status_ms; }
    if (code == 0xd2c1 || code == 0xd2c2 || code == 0xd2c8 || code == 0xd2dd) {
        value = &action_command_status; changed = &action_status_ms;
    }
    if (value && atomic_exchange(value, status) != status)
        atomic_store(changed, (unsigned)(esp_timer_get_time() / 1000));
}

void board_7b_set_recording_status(bool known, bool recording)
{
    unsigned state = !known ? (atomic_load(&recording_state) == 2 ? 2 : 0) : recording ? 2 : 1;
    unsigned previous = atomic_exchange(&recording_state, state);
    if (state == 2 && previous != 2)
        atomic_store(&recording_started_s, (unsigned)(esp_timer_get_time() / 1000000));
}
static const char *command_status(unsigned status)
{
    static const char *const names[] = {"", "PENDING", "APPLIED", "REJECTED", "TIMEOUT", "ACCEPTED"};
    return status <= 5 ? names[status] : "";
}
static void draw_command_status(uint16_t *pixels)
{
    unsigned mode = atomic_load(&mode_command_status), focus = atomic_load(&focus_command_status);
    unsigned action = atomic_load(&action_command_status);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (mode != 1 && (uint32_t)(now - atomic_load(&mode_status_ms)) >= 3000) mode = 0;
    if (focus != 1 && (uint32_t)(now - atomic_load(&focus_status_ms)) >= 3000) focus = 0;
    if (action != 1 && (uint32_t)(now - atomic_load(&action_status_ms)) >= 3000) action = 0;
    if (!atomic_load(&settings_mode)) {
        unsigned level=atomic_load(&info_level);
        if (level==UI_INFO_HIDDEN) return;
        if (level==UI_INFO_COMPACT) {
            if (mode!=3 && mode!=4) mode=0;
            if (focus!=3 && focus!=4) focus=0;
            if (action!=3 && action!=4) action=0;
        }
    }
    char text[80];
    if (!mode && !focus && !action) return;
    if (atomic_load(&settings_mode)) {
        snprintf(text, sizeof(text), "MODE %s", command_status(mode));
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 8, 378, text, 16,
                      mode == 3 || mode == 4 ? 0xf800 : 0x07ff, true, 768);
        if (action) {
            snprintf(text, sizeof(text), "CONTROL %s", command_status(action));
            ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 8, 406, text, 16,
                          action == 3 || action == 4 ? 0xf800 : 0x07ff, true, 768);
        }
        return;
    }
    snprintf(text, sizeof(text), "MODE %s FOCUS %s CONTROL %s", command_status(mode), command_status(focus), command_status(action));
    ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 8, 560, text, 18,
                  mode >= 3 || focus >= 3 ? 0xf800 : 0x07ff, true, 768);
}

static void draw_capture_status(uint16_t *pixels)
{
    char text[64];
    unsigned level=atomic_load(&settings_mode)?UI_INFO_FULL:atomic_load(&info_level);
    ui_overlay_policy_t policy=ui_overlay_policy(level,atomic_load(&camera_battery),atomic_load(&recording_state)==2);
    if (policy.record_dot) {
        for (int y=-5;y<=5;++y) for (int x=-5;x<=5;++x)
            if (x*x+y*y<=25) pixels[(48+y)*BOARD_LCD_WIDTH+14+x]=0xf800;
        if (!policy.record_text) return;
        unsigned seconds = (unsigned)(esp_timer_get_time() / 1000000) - atomic_load(&recording_started_s);
        snprintf(text, sizeof(text), "REC %02u:%02u", seconds / 60, seconds % 60);
        draw_text(pixels, 26, 38, text, 20, 0xf800);
    }
}

static const char *focus_name(unsigned value);

static void draw_preview_status(uint16_t *pixels)
{
    draw_command_status(pixels);
    draw_capture_status(pixels);
    unsigned level=atomic_load(&info_level);
    ui_overlay_policy_t policy=ui_overlay_policy(level,atomic_load(&camera_battery),atomic_load(&recording_state)==2);
    if (!policy.status_bar) {
        char text[40]={0};bool sim=atomic_load(&sim_active);
        if (policy.battery_warning) snprintf(text,sizeof(text),"%sCAM BATTERY %u%%",sim?"SIM ":"",atomic_load(&camera_battery));
        else if (sim) snprintf(text,sizeof(text),"SIM");
        if (*text) {
            int width=ui_fonts_measure(text,18,true)+16,left=BOARD_LCD_WIDTH-width-8;
            int height=ui_fonts_line_height(18)+16;
            for (int y=8;y<8+height;++y) memset(pixels+y*BOARD_LCD_WIDTH+left,0,width*sizeof(uint16_t));
            ui_fonts_draw(pixels,BOARD_LCD_WIDTH,BOARD_LCD_HEIGHT,left+8,16,text,18,policy.battery_warning?0xf800:0xffe0,true,BOARD_LCD_WIDTH-8);
        }
        return;
    }
    char lines[8][32];
    unsigned value = fps_tenths > 999 ? 999 : fps_tenths;
    int rssi = atomic_load(&wifi_rssi);
    unsigned battery = atomic_load(&camera_battery);
    const char *sim = atomic_load(&sim_active) ? "SIM " : "";
    if (battery > 100) snprintf(lines[4], sizeof(lines[4]), "%sBATTERY --",sim);
    else snprintf(lines[4], sizeof(lines[4]), "%sBATTERY %u%%",sim,battery);
    unsigned focus = atomic_load(&prop_focus);
    const char *focus_label = focus_name(focus);
    if (focus_label) snprintf(lines[7], sizeof(lines[7]), "FOCUS %s", focus_label);
    else if (focus >= 0xfffd) snprintf(lines[7], sizeof(lines[7]), "FOCUS --");
    else snprintf(lines[7], sizeof(lines[7]), "FOCUS 0X%04X", focus);
    if (rssi <= -127) snprintf(lines[0], sizeof(lines[0]), "WIFI --");
    else snprintf(lines[0], sizeof(lines[0]), "WIFI %d DBM", rssi);
    snprintf(lines[1], sizeof(lines[1]), "FPS %u.%u", value / 10, value % 10);
    snprintf(lines[2], sizeof(lines[2]), "CAM %s", camera_model);
    snprintf(lines[3], sizeof(lines[3]), "FW %s", camera_firmware);
    unsigned pad_battery = format_controller_battery(lines[5], sizeof(lines[5]));
    format_exposure_mode(lines[6], sizeof(lines[6]), atomic_load(&exposure_mode));

    const int pixel_size = 18, padding = 8, top = 8;
    const int line_height = ui_fonts_line_height(pixel_size) + 2;
    int longest = 0;
    for (int i = 0; i < 8; ++i)
        if (ui_fonts_measure(lines[i], pixel_size, true) > longest)
            longest = ui_fonts_measure(lines[i], pixel_size, true);
    int width = longest + padding * 2;
    if (width > BOARD_LCD_WIDTH - 16) width = BOARD_LCD_WIDTH - 16;
    int height = line_height * 8 + padding * 2;
    int left = BOARD_LCD_WIDTH - 8 - width;
    for (int y = top; y < top + height; ++y)
        memset(pixels + y * BOARD_LCD_WIDTH + left, 0, width * sizeof(uint16_t));
    for (int i = 0; i < 8; ++i)
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
                      left + padding, top + padding + i * line_height, lines[i], pixel_size,
                      (i == 5 ? battery_color(pad_battery) : i == 4 ? battery_color(battery) : i == 0 && rssi > -127 && rssi < -75 ? 0xffe0 : 0xffff),
                      true, BOARD_LCD_WIDTH - 8);
}

static const char *white_balance_name(unsigned value)
{
    switch (value) {
    case 0: case 2: return "AUTO";
    case 1: return "MF";
    case 3: return "ONE PUSH";
    case 4: return "DAYLIGHT";
    case 5: return "FLUORESCENT";
    case 6: return "TUNGSTEN";
    case 7: return "FLASH";
    case 0x8001: return "FLUOR WARM";
    case 0x8002: return "FLUOR COOL";
    case 0x8003: return "FLUOR DAY W";
    case 0x8004: return "FLUOR DAY";
    case 0x8010: return "CLOUDY";
    case 0x8011: return "SHADE";
    case 0x8012: return "COLOR TEMP";
    case 0x8020: return "CUSTOM 1";
    case 0x8021: return "CUSTOM 2";
    case 0x8022: return "CUSTOM 3";
    case 0x8030: return "UNDERWATER";
    default: return NULL;
    }
}

static const char *focus_name(unsigned value)
{
    switch (value) {
    case 1: return "MF";
    case 2: return "AF-S";
    case 3: return "MACRO AF";
    case 0x8004: return "AF-C";
    case 0x8005: return "AF-A";
    case 0x8006: return "DMF";
    case 0x8007: return "MF REVERSE";
    case 0x8008: return "AF-D";
    case 0x8009: return "PF";
    default: return NULL;
    }
}

static const char *meter_name(unsigned value)
{
    switch (value) {
    case 1: return "AVERAGE";
    case 2: return "CENTER AVG";
    case 3: return "MULTI SPOT";
    case 4: return "CENTER SPOT";
    case 0x8001: return "MULTI";
    case 0x8002: return "CENTER";
    case 0x8003: return "SCREEN AVG";
    case 0x8004: return "SPOT STD";
    case 0x8005: return "SPOT LARGE";
    case 0x8006: return "HIGHLIGHT";
    default: return NULL;
    }
}

static const char *flash_name(unsigned value)
{
    switch (value) {
    case 0: case 2: return "OFF";
    case 1: return "AUTO";
    case 3: return "FILL";
    case 4: case 5: return "RED EYE";
    default: return NULL;
    }
}

static void format_named_value(char *text, size_t size, const char *label,
                               unsigned value, unsigned missing,
                               const char *(*name)(unsigned))
{
    const char *decoded = value == missing ? NULL : name(value);
    if (value == missing) snprintf(text, size, "%s --", label);
    else if (decoded) snprintf(text, size, "%s %s", label, decoded);
    else snprintf(text, size, "%s 0X%04X", label, value & 0xffff);
}

static void format_shutter(char *text, size_t size, unsigned value)
{
    if (value == 0xffffffff) {
        snprintf(text, size, "SHUTTER --");
        return;
    }
    if (value == 0) {
        snprintf(text, size, "SHUTTER BULB");
        return;
    }
    unsigned numerator = value >> 16;
    unsigned denominator = value & 0xffff;
    if (!denominator) snprintf(text, size, "SHUTTER 0X%08X", value);
    else if (numerator == 1) snprintf(text, size, "SHUTTER 1/%u", denominator);
    else if (numerator >= denominator)
        snprintf(text, size, "SHUTTER %u.%uS", numerator / denominator,
                 (numerator * 10 / denominator) % 10);
    else snprintf(text, size, "SHUTTER %u/%u", numerator, denominator);
}

static void draw_settings_panel(uint16_t *pixels)
{
    if (atomic_load(&extra_menu_active)) {
        menu_view_t rows[CAMERA_EXTRA_COUNT];
        portENTER_CRITICAL(&menu_mux);
        memcpy(rows, menu_view + 7, sizeof(rows));
        portEXIT_CRITICAL(&menu_mux);
        unsigned selected = atomic_load(&extra_menu_selected);
        for (int y = 0; y < BOARD_LCD_HEIGHT; ++y)
            for (int x = 768; x < BOARD_LCD_WIDTH; ++x)
                pixels[y * BOARD_LCD_WIDTH + x] = x == 768 ? 0x7bef : 0x0841;
        draw_text(pixels, 776, 8, atomic_load(&sim_active) ? "SIM MORE" : "MORE", 20, 0xffff);
        for (unsigned i = 0; i <= CAMERA_EXTRA_COUNT; ++i) {
            int top = 44 + (int)i * 44;
            if (selected == i)
                for (int y = top - 4; y < top + 36; ++y)
                    for (int x = 769; x < BOARD_LCD_WIDTH; ++x) pixels[y * BOARD_LCD_WIDTH + x] = 0x1947;
            char text[40];
            if (i == CAMERA_EXTRA_COUNT) snprintf(text, sizeof(text), "EXIT (A return)");
            else camera_extra_format(i, atomic_load(&prop_extra[i]), text, sizeof(text));
            ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 776, top, text,
                          fit_font_size(text, 18, 240, true),
                          i == CAMERA_EXTRA_COUNT ? 0xffff : rows[i].writable ? 0xffe0 : 0x7bef,
                          true, 1016);
        }
        if (selected < CAMERA_EXTRA_COUNT) {
            menu_view_t v = rows[selected];
            uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
            unsigned status = v.status == 1 || (uint32_t)(now - v.status_ms) < 3000 ? v.status : 0;
            draw_text(pixels, 776, 510, status ? command_status(status) : v.writable ? "LEFT / RIGHT" : "UNAVAILABLE", 16,
                      status == 3 || status == 4 ? 0xf800 : 0x07ff);
            if (v.target_valid) {
                char value[40], target[48];
                camera_extra_format(selected, v.target, value, sizeof(value));
                snprintf(target, sizeof(target), "TO %s", value);
                ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 776, 542, target,
                              fit_font_size(target, 16, 240, true), 0xffe0, true, 1016);
            }
        }
        draw_capture_status(pixels);
        return;
    }
    board_wifi_menu_view_t wifi;
    portENTER_CRITICAL(&wifi_menu_mux); wifi = wifi_menu_view; portEXIT_CRITICAL(&wifi_menu_mux);
    if (wifi.active) {
        for (int y = 0; y < BOARD_LCD_HEIGHT; ++y)
            for (int x = 768; x < BOARD_LCD_WIDTH; ++x) pixels[y * BOARD_LCD_WIDTH + x] = 0x0841;
        ui_fonts_draw(pixels, 1024, 600, 776, 10, "WI-FI HOTSPOT", 20, 0x07ff, true, 1016);
        for (unsigned row = 0; row < 9; ++row) {
            int top = 44 + (int)row * 54;
            if (row == wifi.selected)
                for (int y = top; y < top + 54; ++y)
                    for (int x = 769; x < 1024; ++x) pixels[y * 1024 + x] = 0x1947;
            size_t length = strlen(wifi.lines[row]), offset = 0;
            for (unsigned part = 0; part < 3 && offset < length; ++part) {
                size_t count = length - offset;
                if (count > 28) {
                    count = 28;
                    while (count && ((unsigned char)wifi.lines[row][offset + count] & 0xc0) == 0x80) --count;
                    if (!count) count = 28;
                }
                char text[29]; memcpy(text, wifi.lines[row] + offset, count); text[count] = 0;
                offset += count;
                int size = length > 28 ? 12 : fit_font_size(text, 18, 240, true);
                ui_fonts_draw(pixels, 1024, 600, 776, top + 4 + (int)part * 15, text,
                    size, 0xffe0, true, 1016);
            }
        }
        for (unsigned part = 0; part < 3 && part * 28 < strlen(wifi.footer); ++part) {
            char text[29]; snprintf(text, sizeof(text), "%.28s", wifi.footer + part * 28);
            ui_fonts_draw(pixels, 1024, 600, 776, 536 + (int)part * 17, text, 12, 0x07ff, true, 1016);
        }
        return;
    }
    char lines[18][32];
    unsigned fps = fps_tenths > 999 ? 999 : fps_tenths;
    int rssi = atomic_load(&wifi_rssi);
    unsigned iso = atomic_load(&prop_iso);
    unsigned shutter = atomic_load(&prop_shutter);
    unsigned aperture = atomic_load(&prop_aperture);
    int ev = atomic_load(&prop_ev);

    unsigned battery = atomic_load(&camera_battery);
    const char *sim = atomic_load(&sim_active) ? "SIM " : "";
    snprintf(lines[0], sizeof(lines[0]), rssi <= -127 ? "WIFI --" : "WIFI %d DBM", rssi);
    snprintf(lines[1], sizeof(lines[1]), "FPS %u.%u", fps / 10, fps % 10);
    snprintf(lines[2], sizeof(lines[2]), "CAM %s", camera_model);
    snprintf(lines[3], sizeof(lines[3]), "FW %s", camera_firmware);
    if (battery > 100) snprintf(lines[4], sizeof(lines[4]), "%sBATTERY --",sim);
    else snprintf(lines[4], sizeof(lines[4]), "%sBATTERY %u%%",sim,battery);
    format_exposure_mode(lines[6], sizeof(lines[6]), atomic_load(&exposure_mode));
    format_named_value(lines[7], sizeof(lines[7]), "FOCUS", atomic_load(&prop_focus), 0xffff, focus_name);
    format_shutter(lines[8], sizeof(lines[8]), shutter);
    if (aperture >= 0xfffd) snprintf(lines[9], sizeof(lines[9]), "APERTURE --");
    else snprintf(lines[9], sizeof(lines[9]), "APERTURE F%u.%02u", aperture / 100, aperture % 100);
    if (iso == 0xffffffff || iso == 0x00ffffff) snprintf(lines[10], sizeof(lines[10]), "ISO AUTO");
    else snprintf(lines[10], sizeof(lines[10]), "ISO %u", iso & 0x00ffffff);
    if (ev == INT32_MIN) snprintf(lines[11], sizeof(lines[11]), "EV --");
    else {
        unsigned magnitude = ev < 0 ? (unsigned)(-(int64_t)ev) : (unsigned)ev;
        snprintf(lines[11], sizeof(lines[11]), "EV %c%u.%u", ev < 0 ? '-' : '+',
                 magnitude / 1000, (magnitude % 1000) / 100);
    }
    format_named_value(lines[12], sizeof(lines[12]), "WB", atomic_load(&prop_wb), 0xffff, white_balance_name);
    format_named_value(lines[13], sizeof(lines[13]), "METER", atomic_load(&prop_meter), 0xffff, meter_name);
    format_named_value(lines[14], sizeof(lines[14]), "FLASH", atomic_load(&prop_flash), 0xffff, flash_name);
    snprintf(lines[15], sizeof(lines[15]), "MORE (A enter)");
    unsigned pad_battery = format_controller_battery(lines[5], sizeof(lines[5]));
    snprintf(lines[16], sizeof(lines[16]), "WI-FI >  (A enter)");
    unsigned maintenance=atomic_load(&maint_menu_state);
    snprintf(lines[17],sizeof(lines[17]),"%s",maintenance==1?"STOP LIVE? PRESS A AGAIN":
        maintenance==2?"MAINTENANCE WAIT...":maintenance==3?"MAINTENANCE ON (A off)":
        maintenance==4?"MAINTENANCE FAILED":"MAINTENANCE (A enter)");


    /* The thumbnail occupies 768x432; the lower strip holds extra properties. */
    for (int y = 432; y < BOARD_LCD_HEIGHT; ++y)
        for (int x = 0; x < 768; ++x)
            pixels[y * BOARD_LCD_WIDTH + x] = 0x0841;
    for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i) {
        char text[40];
        camera_extra_format(i, atomic_load(&prop_extra[i]), text, sizeof(text));
        int x = 8 + (i % 2) * 384;
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, x,
                      438 + (i / 2) * 32, text,
                      fit_font_size(text, 18, 368, true), 0xffe0, true, x + 368);
    }
    menu_view_t rows[7];
    portENTER_CRITICAL(&menu_mux);
    memcpy(rows, menu_view, sizeof(rows));
    portEXIT_CRITICAL(&menu_mux);
    unsigned selected = board_7b_menu_selected();
    static const unsigned line_indices[7] = {8, 9, 10, 11, 12, 7, 13};
    const int left = 768, padding = 8, line_height = 28;
    for (int y = 0; y < BOARD_LCD_HEIGHT; ++y)
        for (int x = left; x < BOARD_LCD_WIDTH; ++x)
            pixels[y * BOARD_LCD_WIDTH + x] = x == left ? 0x7bef : 0x0841;
    for (int i = 0; i < 18; ++i) {
        int menu_index = -1;
        for (unsigned j = 0; j < 7; ++j) if (line_indices[j] == (unsigned)i) menu_index = (int)j;
        bool highlight = (menu_index >= 0 && (unsigned)menu_index == selected) || (i == 16 && selected == 7) || (i==17 && selected==8) || (i==15 && selected==9);
        if (highlight) {
            for (int y = 10 + i * line_height; y < 10 + (i + 1) * line_height; ++y)
                for (int x = left + 1; x < BOARD_LCD_WIDTH; ++x) pixels[y * BOARD_LCD_WIDTH + x] = 0x1947;
        }
        uint16_t color = i == 5 ? battery_color(pad_battery) :
                         i == 4 ? battery_color(battery) :
                         i == 0 && rssi > -127 && rssi < -75 ? 0xffe0 : i >= 5 ? 0xffe0 : 0xffff;
        if (menu_index >= 0 && !rows[menu_index].writable) color = 0x7bef;
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, left + padding,
                      12 + i * line_height, lines[i],
                      fit_font_size(lines[i], 18, BOARD_LCD_WIDTH - left - padding * 2, true),
                      color, true,
                      BOARD_LCD_WIDTH - padding);
    }
    draw_command_status(pixels);
    if (selected >= 7) { draw_capture_status(pixels); return; }
    menu_view_t v = rows[selected < 7 ? selected : 0];
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    unsigned status = v.status == 1 || (uint32_t)(now - v.status_ms) < 3000 ? v.status : 0;
    char text[40];
    snprintf(text, sizeof(text), "%s", status ? command_status(status) : v.writable ? "LEFT / RIGHT TO CHANGE" : "UNAVAILABLE");
    ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 776, 542, text, 16,
                  status == 3 || status == 4 ? 0xf800 : 0x07ff, true, 1016);
    if (v.target_valid) {
        char value[32];
        switch (selected) {
        case 0: format_shutter(value, sizeof(value), v.target); break;
        case 1: snprintf(value, sizeof(value), "F%u.%02u", (unsigned)v.target / 100, (unsigned)v.target % 100); break;
        case 2:
            if (v.target == UINT32_MAX || v.target == 0x00ffffff) snprintf(value, sizeof(value), "ISO AUTO");
            else snprintf(value, sizeof(value), "ISO %u", (unsigned)v.target & 0x00ffffff);
            break;
        case 3: {
            int ev_target = (int16_t)v.target;
            unsigned magnitude = ev_target < 0 ? (unsigned)-ev_target : (unsigned)ev_target;
            snprintf(value, sizeof(value), "EV %c%u.%u", ev_target < 0 ? '-' : '+', magnitude / 1000, (magnitude % 1000) / 100);
            break;
        }
        case 4: format_named_value(value, sizeof(value), "WB", v.target, 0xffff, white_balance_name); break;
        case 5: format_named_value(value, sizeof(value), "FOCUS", v.target, 0xffff, focus_name); break;
        default: format_named_value(value, sizeof(value), "METER", v.target, 0xffff, meter_name); break;
        }
        snprintf(text, sizeof(text), "TO %s", value);
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, 776, 570, text, 16, 0x07ff, true, 1016);
    }
    draw_capture_status(pixels);
}

static void draw_maintenance_notice(uint16_t *pixels)
{
    char text[128];
    portENTER_CRITICAL(&wifi_info_mux);memcpy(text,maint_text,sizeof(text));portEXIT_CRITICAL(&wifi_info_mux);
    const char prefix[]="MAINTENANCE OFF:";
    if (strncmp(text,prefix,sizeof(prefix)-1)) return;
    /* The first JPEG replaces the connection screen. Keep the auto-close
     * notice visible on that image too, until maint_ctl expires it. */
    const int top=BOARD_LCD_HEIGHT-52;
    memset(pixels+top*BOARD_LCD_WIDTH,0,52*BOARD_LCD_WIDTH*sizeof(uint16_t));
    ui_fonts_draw(pixels,BOARD_LCD_WIDTH,BOARD_LCD_HEIGHT,12,top+4,
                  "MAINTENANCE OFF: camera connected",16,0xffe0,true,BOARD_LCD_WIDTH-12);
    ui_fonts_draw(pixels,BOARD_LCD_WIDTH,BOARD_LCD_HEIGHT,12,top+26,
                  "Disconnect phone from Wi-Fi.",16,0xffff,true,BOARD_LCD_WIDTH-12);
}

static void record_displayed_frame(void)
{
    int64_t now = esp_timer_get_time();
    if (!fps_last_frame || now - fps_last_frame > 2000000) {
        fps_window_start = now;
        fps_intervals = 0;
        fps_tenths = 0;
    } else if (++fps_intervals && now - fps_window_start >= 1000000) {
        int64_t elapsed = now - fps_window_start;
        fps_tenths = (unsigned)((fps_intervals * 10000000LL + elapsed / 2) / elapsed);
        fps_window_start = now;
        fps_intervals = 0;
    }
    fps_last_frame = now;
}

typedef struct {
    const uint8_t *input;
    size_t size;
    size_t offset;
    uint16_t *pixels;
    unsigned x_offset;
    unsigned y_offset;
} jpeg_context_t;

static UINT jpeg_read(JDEC *decoder, BYTE *buffer, UINT length)
{
    jpeg_context_t *ctx = decoder->device;
    size_t available = ctx->size - ctx->offset;
    if (length > available) length = available;
    if (buffer) memcpy(buffer, ctx->input + ctx->offset, length);
    ctx->offset += length;
    return length;
}

static UINT jpeg_output(JDEC *decoder, void *bitmap, JRECT *rectangle)
{
    jpeg_context_t *ctx = decoder->device;
    if (rectangle->right + ctx->x_offset >= BOARD_LCD_WIDTH ||
        rectangle->bottom + ctx->y_offset >= BOARD_LCD_HEIGHT) return 0;
    // ESP32-S3 ROM TJpgDec produces RGB888 tiles (JD_FORMAT=0).
    const uint8_t *rgb = bitmap;
    for (unsigned y = rectangle->top; y <= rectangle->bottom; ++y) {
        uint16_t *row = ctx->pixels + (y + ctx->y_offset) * BOARD_LCD_WIDTH;
        for (unsigned x = rectangle->left; x <= rectangle->right; ++x) {
            row[x + ctx->x_offset] = ((uint16_t)(rgb[0] & 0xf8) << 8) |
                                    ((uint16_t)(rgb[1] & 0xfc) << 3) | (rgb[2] >> 3);
            rgb += 3;
        }
    }
    return 1;
}

esp_err_t board_7b_show_jpeg(const uint8_t *jpeg, size_t length)
{
    if (!display_mutex || !jpeg) return ESP_ERR_INVALID_ARG;
    if (length < 4) return ESP_ERR_INVALID_RESPONSE;
    int64_t entered = esp_timer_get_time();
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    int64_t locked = esp_timer_get_time();
    if (!board_lcd_ready()) { xSemaphoreGive(display_mutex); return ESP_ERR_INVALID_STATE; }
    // Keep scarce internal RAM available for Wi-Fi and task stacks. The
    // TJpgDec header workspace is not DMA-backed and can safely live in PSRAM.
    if (!jpeg_work) jpeg_work = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpeg_pixels = board_lcd_back_buffer();
    if (!jpeg_work || !jpeg_pixels) {
        xSemaphoreGive(display_mutex);
        return ESP_ERR_NO_MEM;
    }
    jpeg_context_t ctx = {.input = jpeg, .size = length, .pixels = jpeg_pixels};
    JDEC decoder = {0};
    int64_t start = esp_timer_get_time();
    JRESULT result = jd_prepare(&decoder, jpeg_read, jpeg_work, 4096, &ctx);
    esp_err_t err = ESP_ERR_INVALID_RESPONSE;
    if (result == JDR_OK) {
        unsigned scale = 0;
        while (scale < 3 && ((decoder.width >> scale) > BOARD_LCD_WIDTH ||
                            (decoder.height >> scale) > BOARD_LCD_HEIGHT)) ++scale;
        unsigned width = decoder.width >> scale;
        unsigned height = decoder.height >> scale;
        if (!width || !height || width > BOARD_LCD_WIDTH || height > BOARD_LCD_HEIGHT) {
            ESP_LOGE(TAG, "JPEG dimensions not supported: %ux%u", decoder.width, decoder.height);
        } else {
            int64_t header_done = esp_timer_get_time();
            ctx.x_offset = (BOARD_LCD_WIDTH - width) / 2;
            ctx.y_offset = (BOARD_LCD_HEIGHT - height) / 2;
            if (last_width != decoder.width || last_height != decoder.height) {
                ESP_LOGI(TAG, "JPEG header: %ux%u, scale=1/%u, centered at %u,%u",
                         decoder.width, decoder.height, 1U << scale, ctx.x_offset, ctx.y_offset);
                last_width = decoder.width;
                last_height = decoder.height;
            }
            bool settings = atomic_load(&settings_mode);
            bool thumbnail = settings && decoder.width == 1024 && decoder.height == 576;
            if (thumbnail) ctx.x_offset = ctx.y_offset = 0;
            /* A native full-width decode replaces every pixel in its image
             * rectangle, including last frame's overlay. Clear only the black
             * bands, so old text outside the decoded rectangle cannot persist.
             * Thumbnail/ROM paths retain the full clear until their stride and
             * partial edge writes have a separately verified policy. */
            if (!settings && scale == 0 && width == BOARD_LCD_WIDTH &&
                (((uintptr_t)(jpeg_pixels + ctx.y_offset * BOARD_LCD_WIDTH)) & 15) == 0) {
                memset(jpeg_pixels, 0, ctx.y_offset * BOARD_LCD_WIDTH * sizeof(uint16_t));
                unsigned bottom = ctx.y_offset + height;
                memset(jpeg_pixels + bottom * BOARD_LCD_WIDTH, 0,
                       (BOARD_LCD_HEIGHT - bottom) * BOARD_LCD_WIDTH * sizeof(uint16_t));
            } else memset(jpeg_pixels, 0, BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * sizeof(uint16_t));
            int64_t cleared = esp_timer_get_time();
            int64_t stride_us = 0;
            // The same decoder handles both modes; scaling inside the JPEG
            // library needs an extra large buffer that cannot fit liveview RAM.
            if (scale == 0 && width == BOARD_LCD_WIDTH &&
                (((uintptr_t)(jpeg_pixels + ctx.y_offset * BOARD_LCD_WIDTH)) & 15) == 0) {
                if (!fast_decoder) {
                    jpeg_dec_config_t fast_config = DEFAULT_JPEG_DEC_CONFIG();
                    fast_config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
                    if (jpeg_dec_open(&fast_config, &fast_decoder) != JPEG_ERR_OK) {
                        xSemaphoreGive(display_mutex);
                        return ESP_ERR_NO_MEM;
                    }
                }
                jpeg_dec_io_t io = {
                    .inbuf = (uint8_t *)jpeg,
                    .inbuf_len = (int)length,
                    .outbuf = (uint8_t *)(jpeg_pixels + ctx.y_offset * BOARD_LCD_WIDTH),
                };
                jpeg_dec_header_info_t info = {0};
                int output_length = 0;
                jpeg_error_t fast_result = jpeg_dec_parse_header(fast_decoder, &io, &info);
                if (fast_result == JPEG_ERR_OK) fast_result = jpeg_dec_get_outbuf_len(fast_decoder, &output_length);
                if (fast_result != JPEG_ERR_OK || info.width != width || info.height != height ||
                    output_length != (int)(width * height * sizeof(uint16_t))) {
                    ESP_LOGE(TAG, "Fast JPEG header/buffer mismatch: result=%d bytes=%d", fast_result, output_length);
                    if (fast_result == JPEG_ERR_NO_MEM) err = ESP_ERR_NO_MEM;
                    goto finish_decode;
                }
                fast_result = jpeg_dec_process(fast_decoder, &io);
                if (fast_result != JPEG_ERR_OK) {
                    ESP_LOGE(TAG, "Fast JPEG decode failed: %d", fast_result);
                    if (fast_result == JPEG_ERR_NO_MEM) err = ESP_ERR_NO_MEM;
                    goto finish_decode;
                }
                result = JDR_OK;
            } else {
                if (fast_decoder) jpeg_dec_close(fast_decoder);
                fast_decoder = NULL;
                result = jd_decomp(&decoder, jpeg_output, scale);
            }
            if (result == JDR_OK) {
                if (thumbnail) {
                    int64_t shrink_start = esp_timer_get_time();
                    if (!image_shrink(jpeg_pixels, BOARD_LCD_WIDTH*BOARD_LCD_HEIGHT,
                                      1024, 576, BOARD_LCD_WIDTH, 768, 432)) goto finish_decode;
                    for (unsigned y=0; y<432; ++y)
                        memset(jpeg_pixels+y*BOARD_LCD_WIDTH+768,0,(BOARD_LCD_WIDTH-768)*sizeof(uint16_t));
                    memset(jpeg_pixels+432*BOARD_LCD_WIDTH,0,
                           (BOARD_LCD_HEIGHT-432)*BOARD_LCD_WIDTH*sizeof(uint16_t));
                    stride_us=esp_timer_get_time()-shrink_start;
                }
                int64_t decoded = esp_timer_get_time();
                if (esp_timer_get_time() - fps_last_frame > 2000000) fps_tenths = 0;
                if (settings) draw_settings_panel(jpeg_pixels);
                else draw_preview_status(jpeg_pixels);
                draw_maintenance_notice(jpeg_pixels);
                if (!settings) ui_overlay_record_border(jpeg_pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
                                                       atomic_load(&recording_state) == 2);
                int64_t drawn = esp_timer_get_time();
                showing_connection = false;
                err = publish_frame(jpeg_pixels);
                if (err == ESP_OK) record_displayed_frame();
                static int64_t last_profile;
                int64_t finished = esp_timer_get_time();
                if (err == ESP_OK && (!last_profile || finished - last_profile >= 5000000)) {
                    ESP_LOGI(TAG, "JPEG phases us: wait=%lld clear=%lld header=%lld decode=%lld stride=%lld overlay=%lld publish=%lld settings=%u",
                             (long long)(locked-entered), (long long)(cleared-header_done),
                             (long long)(header_done-locked), (long long)(decoded-cleared-stride_us), (long long)stride_us,
                             (long long)(drawn-decoded), (long long)(finished-drawn), settings);
                    last_profile = finished;
                }
                if (err == ESP_OK) ESP_LOGD(TAG, "JPEG DISPLAYED: %u bytes, decode+draw=%ld ms",
                                          (unsigned)length, (long)((esp_timer_get_time() - start) / 1000));
            }
        }
    }
    if (result != JDR_OK) ESP_LOGE(TAG, "JPEG decode failed: TJpgDec=%d", result);
finish_decode:
    if (err == ESP_ERR_INVALID_RESPONSE || err == ESP_ERR_NO_MEM) {
        /* A decoder that stopped mid-scan must not poison the next good frame. */
        if (fast_decoder) jpeg_dec_close(fast_decoder);
        fast_decoder = NULL;
    }
    xSemaphoreGive(display_mutex);
    return err;
}

esp_err_t board_7b_test_jpeg(uint8_t *output, size_t capacity, size_t *length)
{
#if CONFIG_REMOTE_DBG_SIM
    if (!output || !length || capacity > INT32_MAX || !display_mutex) return ESP_ERR_INVALID_ARG;
    *length = 0;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    uint16_t *source = board_lcd_back_buffer();
    jpeg_enc_handle_t encoder = NULL;
    if (!source) goto finished;
    /* The front buffer remains untouched. Reuse the existing aligned back
     * buffer instead of requiring another contiguous 1.125 MiB allocation. */
    for (unsigned y=0; y<576; ++y) for (unsigned x=0; x<1024; ++x) {
        unsigned tile = ((x/32) ^ (y/32)) & 1;
        source[y*1024+x] = ((x*31/1023)<<11) | ((y*63/575)<<5) | (tile?31:0);
    }
    jpeg_enc_config_t config = DEFAULT_JPEG_ENC_CONFIG();
    config.width=1024; config.height=576; config.src_type=JPEG_PIXEL_FORMAT_RGB565_LE;
    config.subsampling=JPEG_SUBSAMPLE_422; config.quality=80;
    jpeg_error_t result = jpeg_enc_open(&config, &encoder);
    if (result == JPEG_ERR_OK) {
        int bytes = 0;
        result = jpeg_enc_process(encoder, (uint8_t*)source, 1024*576*2, output, (int)capacity, &bytes);
        if (result == JPEG_ERR_OK && bytes > 0) { *length = (size_t)bytes; err = ESP_OK; }
        else err = result == JPEG_ERR_NO_MEM ? ESP_ERR_NO_MEM : ESP_FAIL;
    } else err = result == JPEG_ERR_NO_MEM ? ESP_ERR_NO_MEM : ESP_FAIL;
finished:
    if (encoder) jpeg_enc_close(encoder);
    xSemaphoreGive(display_mutex);
    return err;
#else
    (void)output; (void)capacity; (void)length; return ESP_ERR_NOT_SUPPORTED;
#endif
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= 3; ++attempt) {
        err = i2c_master_transmit(expander, data, sizeof(data), 100);
        if (err == ESP_OK) return err;
        ESP_LOGW(TAG, "Expander register 0x%02x retry %u/3: %s", reg, attempt, esp_err_to_name(err));
        if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(20));
    }
    return err;
}

esp_err_t board_7b_init(const char *ssid, const char *password)
{
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    snprintf(connection_ssid, sizeof(connection_ssid), "%s", ssid);
    snprintf(connection_password, sizeof(connection_password), "%s", password);
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus");
    // 7B uses a register-based expander at 0x24. Do not use the 7/CH422G protocol.
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x24,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &expander), TAG, "expander");
    ESP_RETURN_ON_ERROR(write_register(0x02, 0xff), TAG, "expander output mode");
    // EXIO6 LCD power high, EXIO4 SD CS high, EXIO5 USB select low.
    // Keep backlight (EXIO2) off until the framebuffer has been initialized.
    const uint8_t outputs = 0xff & ~(1 << 2) & ~(1 << 5);
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs), TAG, "LCD power");
    ESP_RETURN_ON_ERROR(write_register(0x05, 0), TAG, "backlight PWM");
    vTaskDelay(pdMS_TO_TICKS(100));

    display_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(display_mutex, ESP_ERR_NO_MEM, TAG, "display mutex");
    ESP_RETURN_ON_ERROR(ui_fonts_init(), TAG, "UI fonts");
    ESP_RETURN_ON_ERROR(board_lcd_init(prepare_connection), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs | (1 << 2)), TAG, "backlight on");
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCoreWithCaps(connection_refresh_task, "lcd_status", 32768,
                        NULL, 2, &refresh_task, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "status renderer");
    ESP_LOGI(TAG, "RGB ready: 1024x600, 18MHz, double framebuffer, 30-line bounce, fast JPEG, touch disabled");
    return ESP_OK;
}
