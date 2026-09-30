#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "board_7b.h"
#include "ui_fonts.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "rom/tjpgd.h"
#include "esp_jpeg_dec.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "board_7b";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expander;
static esp_lcd_panel_handle_t panel;
// Used only by the JPEG worker; reuse buffers across frames.
static void *jpeg_work;
static uint16_t *jpeg_pixels;
static unsigned last_width, last_height;
static void *frame_buffers[2];
static unsigned front_buffer;
static SemaphoreHandle_t frame_done;
static bool display_sync_lost;
static jpeg_dec_handle_t fast_decoder;
static jpeg_dec_handle_t scaled_decoder;
static int64_t fps_window_start, fps_last_frame;
static unsigned fps_intervals, fps_tenths;
static char connection_ssid[33], connection_password[65];
static char connection_status_text[96] = "Starting Wi-Fi hotspot...";
static char camera_model[24] = "UNKNOWN";
static char camera_firmware[24] = "UNKNOWN";
static atomic_int wifi_rssi = -127;
static atomic_uint exposure_mode = 0xffffffff;
static atomic_bool settings_mode;
static atomic_uint prop_iso = 0xffffffff;
static atomic_uint prop_shutter = 0xffffffff;
static atomic_uint prop_aperture = 0xffff;
static atomic_int prop_ev = INT32_MIN;
static atomic_uint prop_wb = 0xffff;
static atomic_uint prop_focus = 0xffff;
static atomic_uint prop_meter = 0xffff;
static atomic_uint prop_flash = 0xffff;
static SemaphoreHandle_t display_mutex;
static bool showing_connection = true;
static atomic_bool atom_connected, controller_connected;

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
    const char *title = "easymcucourse camera station";
    draw_text(pixels, 48, 40, title,
              fit_font_size(title, 40, BOARD_LCD_WIDTH - 96, false), 0xffff);
    char line[96];
    snprintf(line, sizeof(line), "SSID: %s", connection_ssid);
    draw_text(pixels, 48, 130, line,
              fit_font_size(line, 32, BOARD_LCD_WIDTH - 96, false), 0xffff);
    snprintf(line, sizeof(line), "Password: %s", connection_password);
    draw_text(pixels, 48, 190, line,
              fit_font_size(line, 32, BOARD_LCD_WIDTH - 96, false), 0xffff);
    snprintf(line, sizeof(line), "Expend unit (ATOM): %s",
             atom_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 270, line, 30, atom_connected ? 0x07e0 : 0xf800);
    snprintf(line, sizeof(line), "Controller (DS4): %s",
             controller_connected ? "Connected" : "Disconnected");
    draw_text(pixels, 48, 330, line, 30, controller_connected ? 0x07e0 : 0xf800);
    draw_text(pixels, 48, 410, status,
              fit_font_size(status, 24, BOARD_LCD_WIDTH - 96, false), 0x07ff);
    draw_text(pixels, 48, 466, "Connect camera to this Wi-Fi.", 24, 0x7bef);
    draw_text(pixels, 48, 516, "Enable PC Remote on camera.", 24, 0x7bef);
}

static esp_err_t publish_frame(uint16_t *pixels)
{
    if (display_sync_lost) return ESP_ERR_INVALID_STATE;
    while (xSemaphoreTake(frame_done, 0) == pdTRUE) {}
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, pixels);
    if (err != ESP_OK) return err;
    // Two completions cover a callback racing with the framebuffer switch.
    for (int i = 0; i < 2; ++i) {
        if (xSemaphoreTake(frame_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
            display_sync_lost = true;
            ESP_LOGE(TAG, "LCD frame synchronization timed out");
            return ESP_ERR_TIMEOUT;
        }
    }
    front_buffer ^= 1;
    return ESP_OK;
}

esp_err_t board_7b_show_connection(const char *status)
{
    if (!panel || !status) return ESP_ERR_INVALID_ARG;
    if (display_sync_lost) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    snprintf(connection_status_text, sizeof(connection_status_text), "%s", status);
    showing_connection = true;
    uint16_t *pixels = frame_buffers[front_buffer ^ 1];
    draw_connection(pixels, connection_status_text);
    fps_last_frame = fps_window_start = 0;
    fps_intervals = fps_tenths = 0;
    esp_err_t err = publish_frame(pixels);
    xSemaphoreGive(display_mutex);
    return err;
}

void board_7b_set_atom_status(bool atom_online, bool controller_online)
{
    if (!panel || !display_mutex) return;
    controller_online = atom_online && controller_online;
    // Steady-state polling must not wait behind JPEG decoding/publication.
    if (atomic_load(&atom_connected) == atom_online &&
        atomic_load(&controller_connected) == controller_online) return;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    bool changed = atom_connected != atom_online || controller_connected != controller_online;
    atom_connected = atom_online;
    controller_connected = controller_online;
    if (changed && showing_connection && !display_sync_lost) {
        uint16_t *pixels = frame_buffers[front_buffer ^ 1];
        draw_connection(pixels, connection_status_text);
        esp_err_t err = publish_frame(pixels);
        if (err != ESP_OK) ESP_LOGW(TAG, "connection status refresh: %s", esp_err_to_name(err));
    }
    xSemaphoreGive(display_mutex);
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
    default: break;
    }
}

bool board_7b_toggle_settings_mode(void)
{
    bool previous = atomic_load(&settings_mode);
    while (!atomic_compare_exchange_weak(&settings_mode, &previous, !previous)) {}
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

static void draw_preview_status(uint16_t *pixels)
{
    char lines[6][32];
    bool controller_online = atomic_load(&controller_connected);
    unsigned value = fps_tenths > 999 ? 999 : fps_tenths;
    int rssi = atomic_load(&wifi_rssi);
    if (rssi <= -127) snprintf(lines[0], sizeof(lines[0]), "WIFI -- DBM");
    else snprintf(lines[0], sizeof(lines[0]), "WIFI %d DBM", rssi);
    snprintf(lines[1], sizeof(lines[1]), "FPS %u.%u", value / 10, value % 10);
    snprintf(lines[2], sizeof(lines[2]), "CAM %s", camera_model);
    snprintf(lines[3], sizeof(lines[3]), "FW %s", camera_firmware);
    format_exposure_mode(lines[4], sizeof(lines[4]), atomic_load(&exposure_mode));
    snprintf(lines[5], sizeof(lines[5]), "DS4 %s", controller_online ? "CONNECTED" : "DISCONNECTED");

    const int pixel_size = 18, padding = 8, top = 8;
    const int line_height = ui_fonts_line_height(pixel_size) + 2;
    int longest = 0;
    for (int i = 0; i < 6; ++i)
        if (ui_fonts_measure(lines[i], pixel_size, true) > longest)
            longest = ui_fonts_measure(lines[i], pixel_size, true);
    int width = longest + padding * 2;
    if (width > BOARD_LCD_WIDTH - 16) width = BOARD_LCD_WIDTH - 16;
    int height = line_height * 6 + padding * 2;
    int left = BOARD_LCD_WIDTH - 8 - width;
    for (int y = top; y < top + height; ++y)
        memset(pixels + y * BOARD_LCD_WIDTH + left, 0, width * sizeof(uint16_t));
    for (int i = 0; i < 6; ++i)
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT,
                      left + padding, top + padding + i * line_height, lines[i], pixel_size,
                      i == 5 ? (controller_online ? 0x07e0 : 0xf800) :
                      (i == 0 && rssi > -127 && rssi < -75 ? 0xffe0 : 0xffff),
                      true, BOARD_LCD_WIDTH - 8);
}

static const char *white_balance_name(unsigned value)
{
    switch (value) {
    case 0: case 2: return "AUTO";
    case 1: return "MANUAL";
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
    case 1: return "MANUAL";
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
    char lines[15][32];
    bool controller_online = atomic_load(&controller_connected);
    unsigned fps = fps_tenths > 999 ? 999 : fps_tenths;
    int rssi = atomic_load(&wifi_rssi);
    unsigned iso = atomic_load(&prop_iso);
    unsigned shutter = atomic_load(&prop_shutter);
    unsigned aperture = atomic_load(&prop_aperture);
    int ev = atomic_load(&prop_ev);

    snprintf(lines[0], sizeof(lines[0]), "CAMERA SETTINGS");
    snprintf(lines[1], sizeof(lines[1]), rssi <= -127 ? "WIFI -- DBM" : "WIFI %d DBM", rssi);
    snprintf(lines[2], sizeof(lines[2]), "FPS %u.%u", fps / 10, fps % 10);
    snprintf(lines[3], sizeof(lines[3]), "CAM %s", camera_model);
    snprintf(lines[4], sizeof(lines[4]), "FW %s", camera_firmware);
    format_exposure_mode(lines[5], sizeof(lines[5]), atomic_load(&exposure_mode));
    if (iso == 0xffffffff || iso == 0x00ffffff) snprintf(lines[6], sizeof(lines[6]), "ISO AUTO");
    else snprintf(lines[6], sizeof(lines[6]), "ISO %u", iso & 0x00ffffff);
    format_shutter(lines[7], sizeof(lines[7]), shutter);
    if (aperture >= 0xfffd) snprintf(lines[8], sizeof(lines[8]), "APERTURE --");
    else snprintf(lines[8], sizeof(lines[8]), "APERTURE F%u.%02u", aperture / 100, aperture % 100);
    if (ev == INT32_MIN) snprintf(lines[9], sizeof(lines[9]), "EV --");
    else {
        unsigned magnitude = ev < 0 ? (unsigned)(-(int64_t)ev) : (unsigned)ev;
        snprintf(lines[9], sizeof(lines[9]), "EV %c%u.%u", ev < 0 ? '-' : '+',
                 magnitude / 1000, (magnitude % 1000) / 100);
    }
    format_named_value(lines[10], sizeof(lines[10]), "WB", atomic_load(&prop_wb), 0xffff, white_balance_name);
    format_named_value(lines[11], sizeof(lines[11]), "FOCUS", atomic_load(&prop_focus), 0xffff, focus_name);
    format_named_value(lines[12], sizeof(lines[12]), "METER", atomic_load(&prop_meter), 0xffff, meter_name);
    format_named_value(lines[13], sizeof(lines[13]), "FLASH", atomic_load(&prop_flash), 0xffff, flash_name);
    snprintf(lines[14], sizeof(lines[14]), "DS4 %s", controller_online ? "CONNECTED" : "DISCONNECTED");

    const int left = 768, padding = 8, line_height = 38;
    for (int y = 0; y < BOARD_LCD_HEIGHT; ++y)
        for (int x = left; x < BOARD_LCD_WIDTH; ++x)
            pixels[y * BOARD_LCD_WIDTH + x] = x == left ? 0x7bef : 0x0841;
    for (int i = 0; i < 15; ++i)
        ui_fonts_draw(pixels, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, left + padding,
                      12 + i * line_height, lines[i],
                      fit_font_size(lines[i], 18, BOARD_LCD_WIDTH - left - padding * 2, true),
                      i == 14 ? (controller_online ? 0x07e0 : 0xf800) :
                      (i == 0 ? 0x07ff : (i >= 5 ? 0xffe0 : 0xffff)), true,
                      BOARD_LCD_WIDTH - padding);
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

static bool frame_complete(esp_lcd_panel_handle_t lcd, const esp_lcd_rgb_panel_event_data_t *event, void *ctx)
{
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(frame_done, &wake);
    return wake == pdTRUE;
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
    if (!panel || !jpeg || length < 4) return ESP_ERR_INVALID_ARG;
    if (display_sync_lost) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    // Keep scarce internal RAM available for Wi-Fi and task stacks. The
    // TJpgDec header workspace is not DMA-backed and can safely live in PSRAM.
    if (!jpeg_work) jpeg_work = heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    jpeg_pixels = frame_buffers[front_buffer ^ 1];
    if (!jpeg_work || !jpeg_pixels) {
        xSemaphoreGive(display_mutex);
        return ESP_ERR_NO_MEM;
    }
    memset(jpeg_pixels, 0, BOARD_LCD_WIDTH * BOARD_LCD_HEIGHT * sizeof(uint16_t));
    jpeg_context_t ctx = {.input = jpeg, .size = length, .pixels = jpeg_pixels};
    JDEC decoder = {0};
    int64_t start = esp_timer_get_time();
    JRESULT result = jd_prepare(&decoder, jpeg_read, jpeg_work, 4096, &ctx);
    esp_err_t err = ESP_FAIL;
    if (result == JDR_OK) {
        unsigned scale = 0;
        while (scale < 3 && ((decoder.width >> scale) > BOARD_LCD_WIDTH ||
                            (decoder.height >> scale) > BOARD_LCD_HEIGHT)) ++scale;
        unsigned width = decoder.width >> scale;
        unsigned height = decoder.height >> scale;
        if (!width || !height || width > BOARD_LCD_WIDTH || height > BOARD_LCD_HEIGHT) {
            ESP_LOGE(TAG, "JPEG dimensions not supported: %ux%u", decoder.width, decoder.height);
        } else {
            ctx.x_offset = (BOARD_LCD_WIDTH - width) / 2;
            ctx.y_offset = (BOARD_LCD_HEIGHT - height) / 2;
            if (last_width != decoder.width || last_height != decoder.height) {
                ESP_LOGI(TAG, "JPEG header: %ux%u, scale=1/%u, centered at %u,%u",
                         decoder.width, decoder.height, 1U << scale, ctx.x_offset, ctx.y_offset);
                last_width = decoder.width;
                last_height = decoder.height;
            }
            bool settings = atomic_load(&settings_mode);
            if (settings && decoder.width == 1024 && decoder.height == 576) {
                // Decode a 3/4-size thumbnail. 768x432 preserves aspect ratio,
                // and both dimensions satisfy the decoder's 8-pixel rule.
                if (!scaled_decoder) {
                    jpeg_dec_config_t scaled_config = DEFAULT_JPEG_DEC_CONFIG();
                    scaled_config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
                    scaled_config.scale.width = 768;
                    scaled_config.scale.height = 432;
                    if (jpeg_dec_open(&scaled_config, &scaled_decoder) != JPEG_ERR_OK) {
                        xSemaphoreGive(display_mutex);
                        return ESP_ERR_NO_MEM;
                    }
                }
                jpeg_dec_io_t io = {
                    .inbuf = (uint8_t *)jpeg,
                    .inbuf_len = (int)length,
                    .outbuf = (uint8_t *)jpeg_pixels,
                };
                jpeg_dec_header_info_t info = {0};
                int output_length = 0;
                jpeg_error_t scaled_result = jpeg_dec_parse_header(scaled_decoder, &io, &info);
                if (scaled_result == JPEG_ERR_OK)
                    scaled_result = jpeg_dec_get_outbuf_len(scaled_decoder, &output_length);
                if (scaled_result != JPEG_ERR_OK || output_length != 768 * 432 * (int)sizeof(uint16_t)) {
                    ESP_LOGE(TAG, "Scaled JPEG setup failed: result=%d bytes=%d", scaled_result, output_length);
                    xSemaphoreGive(display_mutex);
                    return ESP_FAIL;
                }
                scaled_result = jpeg_dec_process(scaled_decoder, &io);
                if (scaled_result != JPEG_ERR_OK) {
                    ESP_LOGE(TAG, "Scaled JPEG decode failed: %d", scaled_result);
                    xSemaphoreGive(display_mutex);
                    return ESP_FAIL;
                }
                // Decoder output is tightly packed. Expand its stride in place,
                // bottom-up, and vertically center it without another 648KiB buffer.
                for (int y = 431; y >= 0; --y)
                    memmove(jpeg_pixels + (84 + y) * BOARD_LCD_WIDTH,
                            jpeg_pixels + y * 768, 768 * sizeof(uint16_t));
                for (int y = 0; y < 84; ++y)
                    memset(jpeg_pixels + y * BOARD_LCD_WIDTH, 0, 768 * sizeof(uint16_t));
                for (int y = 516; y < BOARD_LCD_HEIGHT; ++y)
                    memset(jpeg_pixels + y * BOARD_LCD_WIDTH, 0, 768 * sizeof(uint16_t));
                result = JDR_OK;
            // The camera's 1024-wide image has the same stride as the LCD.
            // Decode RGB565 directly into the aligned back framebuffer.
            } else if (scale == 0 && width == BOARD_LCD_WIDTH &&
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
                    output_length <= 0 || output_length > width * height * sizeof(uint16_t)) {
                    ESP_LOGE(TAG, "Fast JPEG header/buffer mismatch: result=%d bytes=%d", fast_result, output_length);
                    xSemaphoreGive(display_mutex);
                    return ESP_FAIL;
                }
                fast_result = jpeg_dec_process(fast_decoder, &io);
                if (fast_result != JPEG_ERR_OK) {
                    ESP_LOGE(TAG, "Fast JPEG decode failed: %d", fast_result);
                    xSemaphoreGive(display_mutex);
                    return ESP_FAIL;
                }
                result = JDR_OK;
            } else {
                result = jd_decomp(&decoder, jpeg_output, scale);
            }
            if (result == JDR_OK) {
                if (esp_timer_get_time() - fps_last_frame > 2000000) fps_tenths = 0;
                if (settings) draw_settings_panel(jpeg_pixels);
                else draw_preview_status(jpeg_pixels);
                showing_connection = false;
                err = publish_frame(jpeg_pixels);
                if (err == ESP_OK) record_displayed_frame();
                if (err == ESP_OK) ESP_LOGD(TAG, "JPEG DISPLAYED: %u bytes, decode+draw=%ld ms",
                                          (unsigned)length, (long)((esp_timer_get_time() - start) / 1000));
            }
        }
    }
    if (result != JDR_OK) ESP_LOGE(TAG, "JPEG decode failed: TJpgDec=%d", result);
    xSemaphoreGive(display_mutex);
    return err;
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(expander, data, sizeof(data), 100);
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

    // Timings and GPIOs verified against Waveshare's 7B ESP-IDF 06_LCD example.
    const esp_lcd_rgb_panel_config_t config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            // Leave PSRAM bandwidth for simultaneous Wi-Fi RX and JPEG decoding.
            // 30MHz and 40MHz caused unstable live-view scanning on this board.
            .pclk_hz = 18000000,
            .h_res = BOARD_LCD_WIDTH,
            .v_res = BOARD_LCD_HEIGHT,
            .hsync_pulse_width = 162,
            .hsync_back_porch = 152,
            .hsync_front_porch = 48,
            .vsync_pulse_width = 45,
            .vsync_back_porch = 13,
            .vsync_front_porch = 3,
            .flags.pclk_active_neg = true,
        },
        .data_width = 16,
        .bits_per_pixel = 16,
        .num_fbs = 2,
        .bounce_buffer_size_px = BOARD_LCD_WIDTH * 30,
        .dma_burst_size = 64,
        .hsync_gpio_num = 46,
        .vsync_gpio_num = 3,
        .de_gpio_num = 5,
        .pclk_gpio_num = 7,
        .disp_gpio_num = -1,
        .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
        .flags.fb_in_psram = true,
    };
    frame_done = xSemaphoreCreateCounting(4, 0);
    display_mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(frame_done && display_mutex, ESP_ERR_NO_MEM, TAG, "display synchronization");
    ESP_RETURN_ON_ERROR(ui_fonts_init(), TAG, "UI fonts");
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &panel), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &frame_buffers[0], &frame_buffers[1]), TAG, "framebuffers");
    const esp_lcd_rgb_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = frame_complete};
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL), TAG, "frame callback");
    uint16_t *frame = frame_buffers[0];
    draw_connection(frame, "Starting Wi-Fi hotspot...");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, frame), TAG, "connection screen");
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs | (1 << 2)), TAG, "backlight on");
    ESP_LOGI(TAG, "RGB ready: 1024x600, 18MHz, double framebuffer, 30-line bounce, fast JPEG, touch disabled");
    return ESP_OK;
}
