#include "app_ui_internal.h"
#include "ui_model.h"
#include "ui_render.h"
#include "ui_overlay.h"
#include "image_stride.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/tjpgd.h"
#include "esp_jpeg_dec.h"
#if CONFIG_REMOTE_DBG_SIM
#include "esp_jpeg_enc.h"
#endif
#include <string.h>

static const char *TAG = "board_7b";
static void *jpeg_work;
static uint16_t *jpeg_pixels;
static unsigned last_width, last_height;
static jpeg_dec_handle_t fast_decoder;
static int64_t fps_window_start, fps_last_frame;
static unsigned fps_intervals;

void ui_jpeg_reset_fps(void)
{
    fps_last_frame = fps_window_start = 0;
    fps_intervals = ui_model_fps_tenths = 0;
}

/* Allocate before any frames are accepted. Retain resources until reboot. */
esp_err_t ui_jpeg_init(void)
{
    if (jpeg_work || fast_decoder) return ESP_ERR_INVALID_STATE;
    jpeg_work=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!jpeg_work) return ESP_ERR_NO_MEM;
    jpeg_dec_config_t config=DEFAULT_JPEG_DEC_CONFIG();
    config.output_type=JPEG_PIXEL_FORMAT_RGB565_LE;
    if (jpeg_dec_open(&config,&fast_decoder)!=JPEG_ERR_OK) {
        heap_caps_free(jpeg_work); jpeg_work=NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
void ui_jpeg_reset(void)
{
    jpeg_pixels = NULL;
    last_width = last_height = 0;
    ui_jpeg_reset_fps();
}

static void record_displayed_frame(void)
{
    int64_t now = esp_timer_get_time();
    if (!fps_last_frame || now - fps_last_frame > 2000000) {
        fps_window_start = now;
        fps_intervals = 0;
        ui_model_fps_tenths = 0;
    } else if (++fps_intervals && now - fps_window_start >= 1000000) {
        int64_t elapsed = now - fps_window_start;
        ui_model_fps_tenths = (unsigned)((fps_intervals * 10000000LL + elapsed / 2) / elapsed);
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
    if (rectangle->right + ctx->x_offset >= UI_CANVAS_WIDTH ||
        rectangle->bottom + ctx->y_offset >= UI_CANVAS_HEIGHT) return 0;
    // ESP32-S3 ROM TJpgDec produces RGB888 tiles (JD_FORMAT=0).
    const uint8_t *rgb = bitmap;
    for (unsigned y = rectangle->top; y <= rectangle->bottom; ++y) {
        uint16_t *row = ctx->pixels + (y + ctx->y_offset) * UI_CANVAS_WIDTH;
        for (unsigned x = rectangle->left; x <= rectangle->right; ++x) {
            row[x + ctx->x_offset] = ((uint16_t)(rgb[0] & 0xf8) << 8) |
                                    ((uint16_t)(rgb[1] & 0xfc) << 3) | (rgb[2] >> 3);
            rgb += 3;
        }
    }
    return 1;
}

static esp_err_t show_jpeg_open(const uint8_t *jpeg, size_t length)
{
    if (!display_mutex || !jpeg) return ESP_ERR_INVALID_ARG;
    if (length < 4) return ESP_ERR_INVALID_RESPONSE;
    int64_t entered = esp_timer_get_time();
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    int64_t locked = esp_timer_get_time();
    if (!ui_render_surface_ready()) { xSemaphoreGive(display_mutex); return ESP_ERR_INVALID_STATE; }
    // Keep scarce internal RAM available for Wi-Fi and task stacks. The
    // TJpgDec header workspace is not DMA-backed and can safely live in PSRAM.
    if (!jpeg_work || !fast_decoder) { xSemaphoreGive(display_mutex); return ESP_ERR_INVALID_STATE; }
    display_canvas_t canvas = {0};
    esp_err_t acquired = display_canvas_acquire(&canvas, 0);
    if (acquired != ESP_OK) { xSemaphoreGive(display_mutex); return ESP_ERR_INVALID_STATE; }
    jpeg_pixels = canvas.pixels;
    if (!jpeg_work || !jpeg_pixels) {
        display_canvas_cancel(&canvas);
        jpeg_pixels = NULL;
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
        while (scale < 3 && ((decoder.width >> scale) > UI_CANVAS_WIDTH ||
                            (decoder.height >> scale) > UI_CANVAS_HEIGHT)) ++scale;
        unsigned width = decoder.width >> scale;
        unsigned height = decoder.height >> scale;
        if (!width || !height || width > UI_CANVAS_WIDTH || height > UI_CANVAS_HEIGHT) {
            ESP_LOGE(TAG, "JPEG dimensions not supported: %ux%u", decoder.width, decoder.height);
        } else {
            int64_t header_done = esp_timer_get_time();
            ctx.x_offset = (UI_CANVAS_WIDTH - width) / 2;
            ctx.y_offset = (UI_CANVAS_HEIGHT - height) / 2;
            if (last_width != decoder.width || last_height != decoder.height) {
                ESP_LOGI(TAG, "JPEG header: %ux%u, scale=1/%u, centered at %u,%u",
                         decoder.width, decoder.height, 1U << scale, ctx.x_offset, ctx.y_offset);
                last_width = decoder.width;
                last_height = decoder.height;
            }
            bool settings = atomic_load(&ui_model_settings_mode);
            bool thumbnail = settings && decoder.width == 1024 && decoder.height == 576;
            if (thumbnail) ctx.x_offset = ctx.y_offset = 0;
            /* A native full-width decode replaces every pixel in its image
             * rectangle, including last frame's overlay. Clear only the black
             * bands, so old text outside the decoded rectangle cannot persist.
             * Thumbnail/ROM paths retain the full clear until their stride and
             * partial edge writes have a separately verified policy. */
            if (!settings && scale == 0 && width == UI_CANVAS_WIDTH &&
                (((uintptr_t)(jpeg_pixels + ctx.y_offset * UI_CANVAS_WIDTH)) & 15) == 0) {
                memset(jpeg_pixels, 0, ctx.y_offset * UI_CANVAS_WIDTH * sizeof(uint16_t));
                unsigned bottom = ctx.y_offset + height;
                memset(jpeg_pixels + bottom * UI_CANVAS_WIDTH, 0,
                       (UI_CANVAS_HEIGHT - bottom) * UI_CANVAS_WIDTH * sizeof(uint16_t));
            } else memset(jpeg_pixels, 0, UI_CANVAS_WIDTH * UI_CANVAS_HEIGHT * sizeof(uint16_t));
            int64_t cleared = esp_timer_get_time();
            int64_t stride_us = 0;
            // The same decoder handles both modes; scaling inside the JPEG
            // library needs an extra large buffer that cannot fit liveview RAM.
            if (scale == 0 && width == UI_CANVAS_WIDTH &&
                (((uintptr_t)(jpeg_pixels + ctx.y_offset * UI_CANVAS_WIDTH)) & 15) == 0) {
                jpeg_dec_io_t io = {
                    .inbuf = (uint8_t *)jpeg,
                    .inbuf_len = (int)length,
                    .outbuf = (uint8_t *)(jpeg_pixels + ctx.y_offset * UI_CANVAS_WIDTH),
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
                result = jd_decomp(&decoder, jpeg_output, scale);
            }
            if (result == JDR_OK) {
                if (thumbnail) {
                    int64_t shrink_start = esp_timer_get_time();
                    if (!image_shrink(jpeg_pixels, UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT,
                                      1024, 576, UI_CANVAS_WIDTH, 768, 432)) goto finish_decode;
                    for (unsigned y=0; y<432; ++y)
                        memset(jpeg_pixels+y*UI_CANVAS_WIDTH+768,0,(UI_CANVAS_WIDTH-768)*sizeof(uint16_t));
                    memset(jpeg_pixels+432*UI_CANVAS_WIDTH,0,
                           (UI_CANVAS_HEIGHT-432)*UI_CANVAS_WIDTH*sizeof(uint16_t));
                    stride_us=esp_timer_get_time()-shrink_start;
                }
                int64_t decoded = esp_timer_get_time();
                if (esp_timer_get_time() - fps_last_frame > 2000000) ui_model_fps_tenths = 0;
                if (settings) ui_render_draw_settings_panel(jpeg_pixels);
                else ui_render_draw_preview_status(jpeg_pixels);
                if (!settings) ui_overlay_record_border(jpeg_pixels, UI_CANVAS_WIDTH, UI_CANVAS_HEIGHT,
                                                       atomic_load(&ui_model_recording_state) == 2);
                int64_t drawn = esp_timer_get_time();
                showing_connection = false;
                err = ui_render_publish_frame(&canvas);
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
    display_canvas_cancel(&canvas);
    jpeg_pixels = NULL;
    /* Each fast frame reparses its header into the retained decoder. */
    xSemaphoreGive(display_mutex);
    return err;
}

static esp_err_t test_jpeg_open(uint8_t *output, size_t capacity, size_t *length)
{
#if CONFIG_REMOTE_DBG_SIM
    if (!output || !length || capacity > INT32_MAX || !display_mutex) return ESP_ERR_INVALID_ARG;
    *length = 0;
    xSemaphoreTake(display_mutex, portMAX_DELAY);
    esp_err_t err = ESP_ERR_INVALID_STATE;
    display_canvas_t canvas = {0};
    jpeg_enc_handle_t encoder = NULL;
    if (display_canvas_acquire(&canvas, 0) != ESP_OK) goto finished;
    uint16_t *source = canvas.pixels;
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
    display_canvas_cancel(&canvas);
    if (encoder) jpeg_enc_close(encoder);
    xSemaphoreGive(display_mutex);
    return err;
#else
    (void)output; (void)capacity; (void)length; return ESP_ERR_NOT_SUPPORTED;
#endif
}


esp_err_t app_ui_show_jpeg(const uint8_t *jpeg,size_t length)
{
    if (!ui_render_enter()) return ESP_ERR_INVALID_STATE;
    esp_err_t error=show_jpeg_open(jpeg,length);ui_render_leave();return error;
}
esp_err_t app_ui_test_jpeg(uint8_t *output,size_t capacity,size_t *length)
{
    if (!ui_render_enter()) return ESP_ERR_INVALID_STATE;
    esp_err_t error=test_jpeg_open(output,capacity,length);ui_render_leave();return error;
}
