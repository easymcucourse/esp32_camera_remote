#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "board_7b.h"
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
static int64_t fps_window_start, fps_last_frame;
static unsigned fps_intervals, fps_tenths;

// Small built-in 5x7 font: no allocation or UI library in the frame path.
static const uint8_t fps_font[][7] = {
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
    {31,16,16,30,16,16,16}, // F
    {30,17,17,30,16,16,16}, // P
    {15,16,16,14,1,1,30},   // S
    {0,0,0,0,0,12,12},     // .
    {0,0,0,0,0,0,0},       // space
};

static void draw_fps(uint16_t *pixels)
{
    char text[16];
    unsigned value = fps_tenths > 999 ? 999 : fps_tenths;
    snprintf(text, sizeof(text), "FPS %u.%u", value / 10, value % 10);
    const int scale = 3, padding = 6, top = 8;
    int width = (int)strlen(text) * 6 * scale + padding * 2;
    int left = BOARD_LCD_WIDTH - 8 - width;
    for (int y = top; y < top + 7 * scale + padding * 2; ++y)
        memset(pixels + y * BOARD_LCD_WIDTH + left, 0, width * sizeof(uint16_t));
    for (int i = 0; text[i]; ++i) {
        char c = text[i];
        unsigned glyph = c >= '0' && c <= '9' ? c - '0' :
                         c == 'F' ? 10 : c == 'P' ? 11 : c == 'S' ? 12 : c == '.' ? 13 : 14;
        for (int y = 0; y < 7; ++y)
            for (int x = 0; x < 5; ++x)
                if (fps_font[glyph][y] & (1U << (4 - x)))
                    for (int dy = 0; dy < scale; ++dy)
                        for (int dx = 0; dx < scale; ++dx)
                            pixels[(top + padding + y * scale + dy) * BOARD_LCD_WIDTH +
                                   left + padding + i * 6 * scale + x * scale + dx] = 0xffff;
    }
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
    if (!jpeg_work) jpeg_work = heap_caps_malloc(4096, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    jpeg_pixels = frame_buffers[front_buffer ^ 1];
    if (!jpeg_work || !jpeg_pixels) {
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
            // The camera's 1024-wide image has the same stride as the LCD.
            // Decode RGB565 directly into the aligned back framebuffer.
            // Retain the tile decoder for other sizes requiring centering/scaling.
            if (scale == 0 && width == BOARD_LCD_WIDTH &&
                (((uintptr_t)(jpeg_pixels + ctx.y_offset * BOARD_LCD_WIDTH)) & 15) == 0) {
                if (!fast_decoder) {
                    jpeg_dec_config_t fast_config = DEFAULT_JPEG_DEC_CONFIG();
                    fast_config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
                    if (jpeg_dec_open(&fast_config, &fast_decoder) != JPEG_ERR_OK) return ESP_ERR_NO_MEM;
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
                    return ESP_FAIL;
                }
                fast_result = jpeg_dec_process(fast_decoder, &io);
                if (fast_result != JPEG_ERR_OK) {
                    ESP_LOGE(TAG, "Fast JPEG decode failed: %d", fast_result);
                    return ESP_FAIL;
                }
                result = JDR_OK;
            } else {
                result = jd_decomp(&decoder, jpeg_output, scale);
            }
            if (result == JDR_OK) {
                if (esp_timer_get_time() - fps_last_frame > 2000000) fps_tenths = 0;
                draw_fps(jpeg_pixels);
                // Publish the back framebuffer instead of copying into the scanned
                // front buffer. With bounce buffers IDF switches at frame completion.
                while (xSemaphoreTake(frame_done, 0) == pdTRUE) {}
                err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, jpeg_pixels);
                if (err == ESP_OK) {
                    // Two completions cover a callback racing with draw_bitmap's
                    // index update. Only then can the previous front be reused.
                    for (int i = 0; i < 2; ++i) {
                        if (xSemaphoreTake(frame_done, pdMS_TO_TICKS(1000)) != pdTRUE) {
                            display_sync_lost = true;
                            err = ESP_ERR_TIMEOUT;
                            ESP_LOGE(TAG, "LCD frame synchronization timed out");
                            break;
                        }
                    }
                    if (err == ESP_OK) {
                        front_buffer ^= 1;
                        record_displayed_frame();
                    }
                }
                if (err == ESP_OK) ESP_LOGD(TAG, "JPEG DISPLAYED: %u bytes, decode+draw=%ld ms",
                                          (unsigned)length, (long)((esp_timer_get_time() - start) / 1000));
            }
        }
    }
    if (result != JDR_OK) ESP_LOGE(TAG, "JPEG decode failed: TJpgDec=%d", result);
    return err;
}

static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(expander, data, sizeof(data), 100);
}

esp_err_t board_7b_init(void)
{
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
            // 30MHz was stable for stills but caused vertical scan jumps in live view.
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
    ESP_RETURN_ON_FALSE(frame_done, ESP_ERR_NO_MEM, TAG, "frame semaphore");
    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&config, &panel), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &frame_buffers[0], &frame_buffers[1]), TAG, "framebuffers");
    const esp_lcd_rgb_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = frame_complete};
    ESP_RETURN_ON_ERROR(esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL), TAG, "frame callback");
    uint16_t *frame = frame_buffers[0];
    const uint16_t colors[] = {0xffff, 0xffe0, 0x07ff, 0x07e0, 0xf81f, 0xf800, 0x001f, 0x0000};
    for (int y = 0; y < BOARD_LCD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_LCD_WIDTH; ++x) {
            uint16_t color = colors[x * 8 / BOARD_LCD_WIDTH];
            if (y >= 480) {
                unsigned level = x * 255 / (BOARD_LCD_WIDTH - 1);
                color = ((level >> 3) << 11) | ((level >> 2) << 5) | (level >> 3);
            }
            if (x < 4 || y < 4 || x >= BOARD_LCD_WIDTH - 4 || y >= BOARD_LCD_HEIGHT - 4) {
                color = 0xffff;
            }
            frame[y * BOARD_LCD_WIDTH + x] = color;
        }
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(panel), TAG, "panel reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(panel), TAG, "panel init");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, frame), TAG, "pattern");
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs | (1 << 2)), TAG, "backlight on");
    ESP_LOGI(TAG, "RGB ready: 1024x600, 18MHz, double framebuffer, 30-line bounce, fast JPEG, touch disabled");
    return ESP_OK;
}
