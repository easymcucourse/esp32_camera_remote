#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "app_ui_internal.h"
#include "ui_model.h"
#include "ui_render.h"
#include "ui_overlay.h"
#include "display_surface.h"
#include "camera_settings.h"
#include "camera_menu_navigation.h"
#include "ui_fonts.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"

static const char *TAG = "board_7b";

static char connection_status_text[96] = "Starting Wi-Fi hotspot...";



void ui_render_draw_settings_panel(uint16_t *pixels);

SemaphoreHandle_t display_mutex;
static TaskHandle_t refresh_task;
static bool maintenance_published;
bool showing_connection = true;

static void draw_text(uint16_t *pixels, int left, int top, const char *text,
                      int pixel_size, uint16_t color)
{
    ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, left, top, text,
                  pixel_size, color, false, UI_CANVAS_WIDTH);
}

static int fit_font_size(const char *text, int requested, int max_width, bool numbers)
{
    while (requested > 12 && ui_fonts_measure(text, requested, numbers) > max_width)
        --requested;
    return requested;
}

static void draw_connection(uint16_t *pixels, const char *status)
{
    for (int i = 0; i < UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT; ++i) pixels[i] = 0x1082;
    const char *title = "easymcucourse camera console";
    draw_text(pixels, 48, 40, title,
              fit_font_size(title, 40, UI_CANVAS_WIDTH - 96, false), 0xffff);
    if (atomic_load(&ui_model_sim_active)) draw_text(pixels, 48, 92, "SIM", 24, 0xffe0);
    char line[96];
    char ssid[33], password[65], ip[16];
    portENTER_CRITICAL(&ui_model_wifi_info_mux);
    memcpy(ssid, ui_model_connection_ssid, sizeof(ssid));
    memcpy(password, ui_model_connection_password, sizeof(password));
    memcpy(ip, ui_model_connection_ip, sizeof(ip));
    bool default_password=ui_model_connection_default_password;
    portEXIT_CRITICAL(&ui_model_wifi_info_mux);
    snprintf(line, sizeof(line), "SSID: %s", ssid);
    draw_text(pixels, 48, 130, line,
              fit_font_size(line, 32, UI_CANVAS_WIDTH - 96, false), 0xffff);
    snprintf(line, sizeof(line), "Password: %s", password);
    int password_font=fit_font_size(line,32,UI_CANVAS_WIDTH-96-(default_password?160:0),false);
    draw_text(pixels,48,190,line,password_font,0xffff);
    if (default_password) draw_text(pixels,48+ui_fonts_measure(line,password_font,false)+12,190,"DEFAULT",24,0xffe0);
    snprintf(line, sizeof(line), "IP: %s", ip);
    draw_text(pixels, 48, 232, line, 22, 0x7bef);
    snprintf(line, sizeof(line), "Expansion unit (ATOM): %s",
             ui_model_atom_protocol_mismatch ? "Firmware version mismatch" : ui_model_atom_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 270, line, 30, ui_model_atom_connected ? 0x07e0 : 0xf800);
    snprintf(line, sizeof(line), "Controller (DS4): %s",
             ui_model_controller_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 330, line, 30, ui_model_controller_connected ? 0x07e0 : 0xf800);
    static const char *const gimbal_states[] = {"Disabled / disconnected", "Searching", "Connecting", "Connected"};
    unsigned gimbal = atomic_load(&ui_model_gimbal_link_state);
    snprintf(line, sizeof(line), "Gimbal: %s", gimbal_states[gimbal < 4 ? gimbal : 0]);
    draw_text(pixels, 48, 370, line, 22, gimbal == 3 ? 0x07e0 : 0x7bef);
    draw_text(pixels, 48, 410, status,
              fit_font_size(status, 24, UI_CANVAS_WIDTH - 96, false), 0x07ff);
    draw_text(pixels, 48, 466, "Connect camera to this Wi-Fi.", 24, 0x7bef);
    draw_text(pixels, 48, 516, "Enable PC Remote on camera.", 24, 0x7bef);
    if (atomic_load(&ui_model_settings_mode)) ui_render_draw_settings_panel(pixels);
}

bool ui_render_surface_ready(void)
{
    display_surface_status_t status;
    display_surface_get_status(&status);
    return status.ready;
}

static void draw_maintenance(uint16_t *pixels)
{
    memset(pixels,0,UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT*sizeof(*pixels));
    const char *title="MAINTENANCE";const int size=48;
    int width=ui_fonts_measure(title,size,false),height=ui_fonts_line_height(size);
    ui_fonts_draw(pixels,UI_CANVAS_WIDTH,UI_CANVAS_HEIGHT,(UI_CANVAS_WIDTH-width)/2,
        (UI_CANVAS_HEIGHT-height)/2,title,size,0xffff,false,UI_CANVAS_WIDTH);
}
static void prepare_connection(uint16_t *pixels)
{
    if (ui_render_stopping()) { draw_maintenance(pixels);return; }
    draw_connection(pixels, connection_status_text);
}

/* Caller holds display_mutex. Pixel references are cleared before recovery. */
static esp_err_t recover_display(void)
{
    if (atomic_load(&ui_model_display_failed)) return ESP_FAIL;
    if (ui_render_surface_ready()) return ESP_OK;
    ESP_LOGW(TAG, "Recovering display: restarting RGB with boot buffers");
    ui_jpeg_reset();
    if (!showing_connection) atomic_fetch_add(&ui_model_connection_generation, 1);
    showing_connection = true;
    esp_err_t err = display_surface_recover();
    if (err != ESP_OK) {
        atomic_store(&ui_model_display_failed, true);
        ESP_LOGE(TAG, "LCD recovery exhausted: %s; requesting controlled restart", esp_err_to_name(err));
    }
    return err;
}

static esp_err_t recover_display_open(void)
{
    if (!display_mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    esp_err_t err = recover_display();
    xSemaphoreGive(display_mutex);
    return err;
}

bool app_ui_display_failed(void) { return atomic_load(&ui_model_display_failed); }

static esp_err_t test_display_fault_open(unsigned mode)
{
    esp_err_t err = display_surface_test_fault(mode);
    if (err == ESP_OK) { atomic_store(&ui_model_wifi_info_dirty, true); app_ui_refresh_wifi_info(); }
    return err;
}

esp_err_t ui_render_publish_frame(display_canvas_t *canvas)
{
    return display_canvas_refresh(canvas) == ESP_OK ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void app_ui_refresh_wifi_info(void)
{
    /* Wi-Fi/NVS and input workers never render or wait behind a stalled LCD. */
    if (!ui_render_enter()) return;
    if (refresh_task) xTaskNotifyGive(refresh_task);
    ui_render_leave();
}

static void connection_refresh_task(void *arg)
{
    (void)arg;
    while (!ui_render_stopping()) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(250));
        if (ui_render_stopping()) break;
        if (!atomic_load(&ui_model_wifi_info_dirty) || atomic_load(&ui_model_display_failed)) continue;
        if (!ui_render_enter()) break;
        if (xSemaphoreTake(display_mutex, 0) != pdTRUE) { ui_render_leave();continue; }
        atomic_store(&ui_model_wifi_info_dirty, false);
        if (showing_connection) {
            esp_err_t err = recover_display();
            if (err == ESP_OK) {
                display_canvas_t canvas = {0};
                err = display_canvas_acquire(&canvas, 0);
                if (err == ESP_OK) {
                    draw_connection(canvas.pixels, connection_status_text);
                    err = ui_render_publish_frame(&canvas);
                }
                if (err != ESP_OK) recover_display();
            }
        }
        xSemaphoreGive(display_mutex);ui_render_leave();
    }
    while (!ui_render_idle()) vTaskDelay(pdMS_TO_TICKS(10));
    refresh_task=NULL;ui_render_refresh_set(false);vTaskDeleteWithCaps(NULL);
}

static esp_err_t show_connection_open(const char *status)
{
    if (!display_mutex || !status) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    snprintf(connection_status_text, sizeof(connection_status_text), "%s", status);
    if (!showing_connection) atomic_fetch_add(&ui_model_connection_generation, 1);
    showing_connection = true;
    esp_err_t err = recover_display();
    if (err != ESP_OK) { xSemaphoreGive(display_mutex); return err; }
    display_canvas_t canvas = {0};
    err = display_canvas_acquire(&canvas, 0);
    if (err != ESP_OK) { xSemaphoreGive(display_mutex); return err; }
    draw_connection(canvas.pixels, connection_status_text);
    ui_jpeg_reset_fps();
    err = ui_render_publish_frame(&canvas);
    if (err != ESP_OK) err = recover_display();
    xSemaphoreGive(display_mutex);
    return err;
}

static unsigned format_controller_battery(char *text, size_t size)
{
    unsigned state = atomic_load(&ui_model_controller_battery);
    unsigned percent = atomic_load(&ui_model_controller_connected) ? state & 255u : 255u;
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

static const char *command_status(unsigned status)
{
    static const char *const names[] = {"", "PENDING", "APPLIED", "REJECTED", "TIMEOUT", "ACCEPTED"};
    return status <= 5 ? names[status] : "";
}
static void draw_command_status(uint16_t *pixels)
{
    unsigned mode = atomic_load(&ui_model_mode_command_status), focus = atomic_load(&ui_model_focus_command_status);
    unsigned action = atomic_load(&ui_model_action_command_status);
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    if (mode != 1 && (uint32_t)(now - atomic_load(&ui_model_mode_status_ms)) >= 3000) mode = 0;
    if (focus != 1 && (uint32_t)(now - atomic_load(&ui_model_focus_status_ms)) >= 3000) focus = 0;
    if (action != 1 && (uint32_t)(now - atomic_load(&ui_model_action_status_ms)) >= 3000) action = 0;
    if (!atomic_load(&ui_model_settings_mode)) {
        unsigned level=atomic_load(&ui_model_info_level);
        if (level==UI_INFO_HIDDEN) return;
        if (level==UI_INFO_COMPACT) {
            if (mode!=3 && mode!=4) mode=0;
            if (focus!=3 && focus!=4) focus=0;
            if (action!=3 && action!=4) action=0;
        }
    }
    char text[80];
    if (!mode && !focus && !action) return;
    if (atomic_load(&ui_model_settings_mode)) {
        snprintf(text, sizeof(text), "MODE %s", command_status(mode));
        ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 8, 378, text, 16,
                      mode == 3 || mode == 4 ? 0xf800 : 0x07ff, true, 768);
        if (action) {
            snprintf(text, sizeof(text), "CONTROL %s", command_status(action));
            ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 8, 406, text, 16,
                          action == 3 || action == 4 ? 0xf800 : 0x07ff, true, 768);
        }
        return;
    }
    snprintf(text, sizeof(text), "MODE %s FOCUS %s CONTROL %s", command_status(mode), command_status(focus), command_status(action));
    ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 8, 560, text, 18,
                  mode >= 3 || focus >= 3 ? 0xf800 : 0x07ff, true, 768);
}

static void draw_capture_status(uint16_t *pixels)
{
    char text[64];
    unsigned level=atomic_load(&ui_model_settings_mode)?UI_INFO_FULL:atomic_load(&ui_model_info_level);
    ui_overlay_policy_t policy=ui_overlay_policy(level,atomic_load(&ui_model_camera_battery),atomic_load(&ui_model_recording_state)==2);
    if (policy.record_dot) {
        for (int y=-5;y<=5;++y) for (int x=-5;x<=5;++x)
            if (x*x+y*y<=25) pixels[(48+y)*UI_CANVAS_WIDTH+14+x]=0xf800;
        if (!policy.record_text) return;
        unsigned seconds = (unsigned)(esp_timer_get_time() / 1000000) - atomic_load(&ui_model_recording_started_s);
        snprintf(text, sizeof(text), "REC %02u:%02u", seconds / 60, seconds % 60);
        draw_text(pixels, 26, 38, text, 20, 0xf800);
    }
}

static const char *focus_name(unsigned value);

void ui_render_draw_preview_status(uint16_t *pixels)
{
    draw_command_status(pixels);
    draw_capture_status(pixels);
    unsigned level=atomic_load(&ui_model_info_level);
    ui_overlay_policy_t policy=ui_overlay_policy(level,atomic_load(&ui_model_camera_battery),atomic_load(&ui_model_recording_state)==2);
    if (!policy.status_bar) {
        char text[40]={0};bool sim=atomic_load(&ui_model_sim_active);
        if (policy.battery_warning) snprintf(text,sizeof(text),"%sCAM BATTERY %u%%",sim?"SIM ":"",atomic_load(&ui_model_camera_battery));
        else if (sim) snprintf(text,sizeof(text),"SIM");
        if (*text) {
            int width=ui_fonts_measure(text,18,true)+16,left=UI_CANVAS_WIDTH-width-8;
            int height=ui_fonts_line_height(18)+16;
            for (int y=8;y<8+height;++y) memset(pixels+y*UI_CANVAS_WIDTH+left,0,width*sizeof(uint16_t));
            ui_fonts_draw(pixels,UI_CANVAS_WIDTH,UI_CANVAS_HEIGHT,left+8,16,text,18,policy.battery_warning?0xf800:0xffe0,true,UI_CANVAS_WIDTH-8);
        }
        return;
    }
    char lines[8][32];
    unsigned value = ui_model_fps_tenths > 999 ? 999 : ui_model_fps_tenths;
    int rssi = atomic_load(&ui_model_wifi_rssi);
    unsigned battery = atomic_load(&ui_model_camera_battery);
    const char *sim = atomic_load(&ui_model_sim_active) ? "SIM " : "";
    if (battery > 100) snprintf(lines[4], sizeof(lines[4]), "%sBATTERY --",sim);
    else snprintf(lines[4], sizeof(lines[4]), "%sBATTERY %u%%",sim,battery);
    unsigned focus = atomic_load(&ui_model_prop_focus);
    const char *focus_label = focus_name(focus);
    if (focus_label) snprintf(lines[7], sizeof(lines[7]), "FOCUS %s", focus_label);
    else if (focus >= 0xfffd) snprintf(lines[7], sizeof(lines[7]), "FOCUS --");
    else snprintf(lines[7], sizeof(lines[7]), "FOCUS 0X%04X", focus);
    if (rssi <= -127) snprintf(lines[0], sizeof(lines[0]), "WIFI --");
    else snprintf(lines[0], sizeof(lines[0]), "WIFI %d DBM", rssi);
    snprintf(lines[1], sizeof(lines[1]), "FPS %u.%u", value / 10, value % 10);
    snprintf(lines[2], sizeof(lines[2]), "CAM %s", ui_model_camera_model);
    snprintf(lines[3], sizeof(lines[3]), "FW %s", ui_model_camera_firmware);
    unsigned pad_battery = format_controller_battery(lines[5], sizeof(lines[5]));
    format_exposure_mode(lines[6], sizeof(lines[6]), atomic_load(&ui_model_exposure_mode));

    const int pixel_size = 18, padding = 8, top = 8;
    const int line_height = ui_fonts_line_height(pixel_size) + 2;
    int longest = 0;
    for (int i = 0; i < 8; ++i)
        if (ui_fonts_measure(lines[i], pixel_size, true) > longest)
            longest = ui_fonts_measure(lines[i], pixel_size, true);
    int width = longest + padding * 2;
    if (width > UI_CANVAS_WIDTH - 16) width = UI_CANVAS_WIDTH - 16;
    int height = line_height * 8 + padding * 2;
    int left = UI_CANVAS_WIDTH - 8 - width;
    for (int y = top; y < top + height; ++y)
        memset(pixels + y * UI_CANVAS_WIDTH + left, 0, width * sizeof(uint16_t));
    for (int i = 0; i < 8; ++i)
        ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT,
                      left + padding, top + padding + i * line_height, lines[i], pixel_size,
                      (i == 5 ? battery_color(pad_battery) : i == 4 ? battery_color(battery) : i == 0 && rssi > -127 && rssi < -75 ? 0xffe0 : 0xffff),
                      true, UI_CANVAS_WIDTH - 8);
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

void ui_render_draw_settings_panel(uint16_t *pixels)
{
    if (atomic_load(&ui_model_extra_menu_active)) {
        menu_view_t rows[CAMERA_EXTRA_COUNT];
        portENTER_CRITICAL(&ui_model_menu_mux);
        memcpy(rows, ui_model_menu_view + 7, sizeof(rows));
        portEXIT_CRITICAL(&ui_model_menu_mux);
        unsigned selected = atomic_load(&ui_model_extra_menu_selected);
        for (int y = 0; y < UI_CANVAS_HEIGHT; ++y)
            for (int x = 768; x < UI_CANVAS_WIDTH; ++x)
                pixels[y * UI_CANVAS_WIDTH + x] = x == 768 ? 0x7bef : 0x0841;
        draw_text(pixels, 776, 8, atomic_load(&ui_model_sim_active) ? "SIM MORE" : "MORE", 20, 0xffff);
        for (unsigned i = 0; i <= CAMERA_EXTRA_COUNT; ++i) {
            int top = 44 + (int)i * 44;
            if (selected == i)
                for (int y = top - 4; y < top + 36; ++y)
                    for (int x = 769; x < UI_CANVAS_WIDTH; ++x) pixels[y * UI_CANVAS_WIDTH + x] = 0x1947;
            char text[40];
            if (i == CAMERA_EXTRA_COUNT) snprintf(text, sizeof(text), "EXIT (A return)");
            else camera_extra_format(i, atomic_load(&ui_model_prop_extra[i]), text, sizeof(text));
            ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 776, top, text,
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
                ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 776, 542, target,
                              fit_font_size(target, 16, 240, true), 0xffe0, true, 1016);
            }
        }
        draw_capture_status(pixels);
        return;
    }
    char lines[17][32];
    unsigned fps = ui_model_fps_tenths > 999 ? 999 : ui_model_fps_tenths;
    int rssi = atomic_load(&ui_model_wifi_rssi);
    unsigned iso = atomic_load(&ui_model_prop_iso);
    unsigned shutter = atomic_load(&ui_model_prop_shutter);
    unsigned aperture = atomic_load(&ui_model_prop_aperture);
    int ev = atomic_load(&ui_model_prop_ev);

    unsigned battery = atomic_load(&ui_model_camera_battery);
    const char *sim = atomic_load(&ui_model_sim_active) ? "SIM " : "";
    snprintf(lines[0], sizeof(lines[0]), rssi <= -127 ? "WIFI --" : "WIFI %d DBM", rssi);
    snprintf(lines[1], sizeof(lines[1]), "FPS %u.%u", fps / 10, fps % 10);
    snprintf(lines[2], sizeof(lines[2]), "CAM %s", ui_model_camera_model);
    snprintf(lines[3], sizeof(lines[3]), "FW %s", ui_model_camera_firmware);
    if (battery > 100) snprintf(lines[4], sizeof(lines[4]), "%sBATTERY --",sim);
    else snprintf(lines[4], sizeof(lines[4]), "%sBATTERY %u%%",sim,battery);
    format_exposure_mode(lines[6], sizeof(lines[6]), atomic_load(&ui_model_exposure_mode));
    format_named_value(lines[7], sizeof(lines[7]), "FOCUS", atomic_load(&ui_model_prop_focus), 0xffff, focus_name);
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
    format_named_value(lines[12], sizeof(lines[12]), "WB", atomic_load(&ui_model_prop_wb), 0xffff, white_balance_name);
    format_named_value(lines[13], sizeof(lines[13]), "METER", atomic_load(&ui_model_prop_meter), 0xffff, meter_name);
    format_named_value(lines[14], sizeof(lines[14]), "FLASH", atomic_load(&ui_model_prop_flash), 0xffff, flash_name);
    snprintf(lines[15], sizeof(lines[15]), "MORE (A enter)");
    unsigned pad_battery = format_controller_battery(lines[5], sizeof(lines[5]));
    snprintf(lines[16], sizeof(lines[16]), "WI-FI (Web settings)");
    /* The thumbnail occupies 768x432; the lower strip holds extra properties. */
    for (int y = 432; y < UI_CANVAS_HEIGHT; ++y)
        for (int x = 0; x < 768; ++x)
            pixels[y * UI_CANVAS_WIDTH + x] = 0x0841;
    for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i) {
        char text[40];
        camera_extra_format(i, atomic_load(&ui_model_prop_extra[i]), text, sizeof(text));
        int x = 8 + (i % 2) * 384;
        ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, x,
                      438 + (i / 2) * 32, text,
                      fit_font_size(text, 18, 368, true), 0xffe0, true, x + 368);
    }
    menu_view_t rows[7];
    portENTER_CRITICAL(&ui_model_menu_mux);
    memcpy(rows, ui_model_menu_view, sizeof(rows));
    portEXIT_CRITICAL(&ui_model_menu_mux);
    unsigned selected = app_ui_menu_selected();
    static const unsigned line_indices[7] = {8, 9, 10, 11, 12, 7, 13};
    const int left = 768, padding = 8, line_height = 28;
    for (int y = 0; y < UI_CANVAS_HEIGHT; ++y)
        for (int x = left; x < UI_CANVAS_WIDTH; ++x)
            pixels[y * UI_CANVAS_WIDTH + x] = x == left ? 0x7bef : 0x0841;
    for (int i = 0; i < 17; ++i) {
        int menu_index = -1;
        for (unsigned j = 0; j < 7; ++j) if (line_indices[j] == (unsigned)i) menu_index = (int)j;
        bool highlight = (menu_index >= 0 && (unsigned)menu_index == selected) || (i == 16 && selected == 7) || (i==15 && selected==9);
        if (highlight) {
            for (int y = 10 + i * line_height; y < 10 + (i + 1) * line_height; ++y)
                for (int x = left + 1; x < UI_CANVAS_WIDTH; ++x) pixels[y * UI_CANVAS_WIDTH + x] = 0x1947;
        }
        uint16_t color = i == 5 ? battery_color(pad_battery) :
                         i == 4 ? battery_color(battery) :
                         i == 0 && rssi > -127 && rssi < -75 ? 0xffe0 : i >= 5 ? 0xffe0 : 0xffff;
        if (menu_index >= 0 && !rows[menu_index].writable) color = 0x7bef;
        ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, left + padding,
                      12 + i * line_height, lines[i],
                      fit_font_size(lines[i], 18, UI_CANVAS_WIDTH - left - padding * 2, true),
                      color, true,
                      UI_CANVAS_WIDTH - padding);
    }
    draw_command_status(pixels);
    if (selected >= 7) { draw_capture_status(pixels); return; }
    menu_view_t v = rows[selected < 7 ? selected : 0];
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
    unsigned status = v.status == 1 || (uint32_t)(now - v.status_ms) < 3000 ? v.status : 0;
    char text[40];
    snprintf(text, sizeof(text), "%s", status ? command_status(status) : v.writable ? "LEFT / RIGHT TO CHANGE" : "UNAVAILABLE");
    ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 776, 542, text, 16,
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
        ui_fonts_draw(pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT, 776, 570, text, 16, 0x07ff, true, 1016);
    }
    draw_capture_status(pixels);
}

esp_err_t app_ui_init(const char *ssid, const char *password)
{
    if (!ssid || !password) return ESP_ERR_INVALID_ARG;
    if (display_mutex) return ESP_ERR_INVALID_STATE;
    snprintf(ui_model_connection_ssid, sizeof(ui_model_connection_ssid), "%s", ssid);
    snprintf(ui_model_connection_password, sizeof(ui_model_connection_password), "%s", password);
    display_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(display_mutex, ESP_ERR_NO_MEM, TAG, "display mutex");
    ESP_RETURN_ON_ERROR(display_surface_init(prepare_connection, ui_fonts_init), TAG, "display surface");
    display_surface_status_t surface;
    display_surface_get_status(&surface);
    ESP_RETURN_ON_FALSE(surface.width == UI_CANVAS_WIDTH && surface.height == UI_CANVAS_HEIGHT &&
                        surface.stride_pixels == UI_CANVAS_WIDTH, ESP_ERR_NOT_SUPPORTED, TAG,
                        "UI requires a 1024x600 tightly packed RGB565 canvas");
    ESP_RETURN_ON_ERROR(ui_jpeg_init(), TAG, "boot JPEG resources");
    ui_render_refresh_set(true);
    BaseType_t created=xTaskCreatePinnedToCoreWithCaps(connection_refresh_task, "lcd_status", 32768,
                        NULL, 2, &refresh_task, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (created!=pdPASS) { ui_render_refresh_set(false);return ESP_ERR_NO_MEM; }
    ESP_LOGI(TAG, "RGB ready: 1024x600, 18MHz, double framebuffer, fast JPEG, touch disabled");
    return ESP_OK;
}

esp_err_t app_ui_recover_display(void)
{
    if (!ui_render_enter()) return ESP_ERR_INVALID_STATE;
    esp_err_t result=recover_display_open();ui_render_leave();return result;
}
esp_err_t app_ui_test_display_fault(unsigned mode)
{
    if (!ui_render_enter()) return ESP_ERR_INVALID_STATE;
    esp_err_t result=test_display_fault_open(mode);ui_render_leave();return result;
}
esp_err_t app_ui_show_connection(const char *status)
{
    if (!ui_render_enter()) return ESP_ERR_INVALID_STATE;
    esp_err_t result=show_connection_open(status);ui_render_leave();return result;
}
static uint32_t render_remaining(int64_t deadline)
{
    int64_t remaining=deadline-esp_timer_get_time();
    return remaining>0 ? (uint32_t)((remaining+999)/1000) : 0;
}
bool app_ui_renderer_quiesce(uint32_t timeout_ms)
{
    if (!display_mutex) return false;
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    if (!ui_render_close(deadline)) return false;
    if (xSemaphoreTake(display_mutex,pdMS_TO_TICKS(render_remaining(deadline)))!=pdTRUE) return false;
    ui_jpeg_reset();xSemaphoreGive(display_mutex);
    return esp_timer_get_time()<=deadline;
}
esp_err_t app_ui_enter_maintenance(uint32_t timeout_ms)
{
    if (maintenance_published) return ESP_OK;
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    if (!app_ui_renderer_quiesce(timeout_ms)) return ESP_ERR_TIMEOUT;
    ui_model_freeze_and_clear();
    if (!render_remaining(deadline)) return ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(display_mutex,pdMS_TO_TICKS(render_remaining(deadline)))!=pdTRUE) return ESP_ERR_TIMEOUT;
    memset(connection_status_text,0,sizeof(connection_status_text));
    showing_connection=false;
    esp_err_t error=recover_display();
    display_canvas_t canvas={0};
    if (error==ESP_OK) error=display_canvas_acquire(&canvas,render_remaining(deadline));
    if (error==ESP_OK) {
        draw_maintenance(canvas.pixels);error=ui_render_publish_frame(&canvas);
        if (error==ESP_OK) maintenance_published=true;
    }
    display_canvas_cancel(&canvas);xSemaphoreGive(display_mutex);
    return error;
}
