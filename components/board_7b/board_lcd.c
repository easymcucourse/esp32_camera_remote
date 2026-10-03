#include "board_lcd.h"
#include "board_7b.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include <stdatomic.h>

static const char *TAG = "board_lcd";
static esp_lcd_panel_handle_t panel;
static void *buffers[2];
static unsigned front;
static bool ready;
static SemaphoreHandle_t frame_done;
static atomic_uint debug_fault;
esp_err_t board_lcd_test_fault(unsigned mode)
{
#if CONFIG_APP_DEBUG_FAULT_INJECTION
    if (mode > 2) return ESP_ERR_INVALID_ARG;
    atomic_store(&debug_fault, mode); return ESP_OK;
#else
    (void)mode; return ESP_ERR_NOT_SUPPORTED;
#endif
}

/* Timings/GPIOs verified against Waveshare's 7B ESP-IDF 06_LCD example.
 * 18 MHz leaves PSRAM bandwidth for concurrent Wi-Fi and JPEG decoding. */
static const esp_lcd_rgb_panel_config_t config = {
    .clk_src = LCD_CLK_SRC_DEFAULT,
    .timings = {
        .pclk_hz = 18000000, .h_res = BOARD_LCD_WIDTH, .v_res = BOARD_LCD_HEIGHT,
        .hsync_pulse_width = 162, .hsync_back_porch = 152, .hsync_front_porch = 48,
        .vsync_pulse_width = 45, .vsync_back_porch = 13, .vsync_front_porch = 3,
        .flags.pclk_active_neg = true,
    },
    .data_width = 16, .bits_per_pixel = 16, .num_fbs = 2,
    .bounce_buffer_size_px = BOARD_LCD_WIDTH * 30, .dma_burst_size = 64,
    .hsync_gpio_num = 46, .vsync_gpio_num = 3, .de_gpio_num = 5, .pclk_gpio_num = 7,
    .disp_gpio_num = -1,
    .data_gpio_nums = {14, 38, 18, 17, 10, 39, 0, 45, 48, 47, 21, 1, 2, 42, 41, 40},
    .flags.fb_in_psram = true,
};

static bool frame_complete(esp_lcd_panel_handle_t lcd,
                           const esp_lcd_rgb_panel_event_data_t *event, void *ctx)
{
    (void)lcd; (void)event; (void)ctx;
    if (atomic_load(&debug_fault)) return false;
    BaseType_t wake = pdFALSE;
    xSemaphoreGiveFromISR(frame_done, &wake);
    return wake == pdTRUE;
}

static void drain_completions(void)
{
    while (xSemaphoreTake(frame_done, 0) == pdTRUE) {}
}

static esp_err_t wait_frame(void)
{
    /* Two completions cover a callback racing with the framebuffer switch. */
    for (unsigned i = 0; i < 2; ++i)
        if (xSemaphoreTake(frame_done, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

static esp_err_t delete_panel(void)
{
    ready = false;
    if (panel) {
        esp_err_t err = esp_lcd_panel_del(panel);
        if (err != ESP_OK) return err; /* Never recreate while the old panel is live. */
        panel = NULL;
        if (atomic_load(&debug_fault) == 1) atomic_store(&debug_fault, 0);
    }
    buffers[0] = buffers[1] = NULL;
    front = 0;
    drain_completions();
    return ESP_OK;
}

static esp_err_t create_panel(board_lcd_prepare_t prepare)
{
    esp_err_t err = esp_lcd_new_rgb_panel(&config, &panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buffers[0], &buffers[1]);
    if (err != ESP_OK) return err;
    const esp_lcd_rgb_panel_event_callbacks_t callbacks = {.on_frame_buf_complete = frame_complete};
    err = esp_lcd_rgb_panel_register_event_callbacks(panel, &callbacks, NULL);
    if (err != ESP_OK) return err;
    /* Neither buffer is being scanned yet. Both must be initialized before init. */
    prepare(buffers[0]); prepare(buffers[1]);
    err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) return err;
    err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, buffers[0]);
    if (err != ESP_OK) return err;
    drain_completions();
    err = esp_lcd_panel_init(panel);
    if (err == ESP_OK) err = wait_frame();
    if (err == ESP_OK) { front = 0; ready = true; }
    return err;
}

esp_err_t board_lcd_init(board_lcd_prepare_t prepare)
{
    if (!prepare || frame_done) return ESP_ERR_INVALID_STATE;
    frame_done = xSemaphoreCreateCounting(4, 0);
    if (!frame_done) return ESP_ERR_NO_MEM;
    esp_err_t err = create_panel(prepare);
    if (err != ESP_OK) delete_panel();
    return err;
}

bool board_lcd_ready(void) { return ready; }
uint16_t *board_lcd_back_buffer(void) { return ready ? buffers[front ^ 1] : NULL; }

esp_err_t board_lcd_publish(uint16_t *pixels)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (!pixels || (pixels != buffers[0] && pixels != buffers[1])) return ESP_ERR_INVALID_ARG;
    drain_completions();
    ready = false; /* Ownership is unknown until both completions have arrived. */
    esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, pixels);
    if (err == ESP_OK) err = wait_frame();
    if (err == ESP_OK) { front = pixels == buffers[1]; ready = true; }
    else ESP_LOGE(TAG, "LCD publication failed: %s; framebuffer writes suspended", esp_err_to_name(err));
    return err;
}

static esp_err_t restart_panel(board_lcd_prepare_t prepare)
{
    if (!panel || !buffers[0] || !buffers[1]) return ESP_ERR_INVALID_STATE;
    /* init restarts RGB/GDMA. Never touch pixel memory until the known front
     * has completed twice; retaining allocations avoids runtime fragmentation. */
    esp_err_t err = esp_lcd_panel_reset(panel);
    if (err != ESP_OK) return err;
    if (atomic_load(&debug_fault) == 1) atomic_store(&debug_fault, 0);
    err = esp_lcd_panel_draw_bitmap(panel, 0, 0, BOARD_LCD_WIDTH, BOARD_LCD_HEIGHT, buffers[front]);
    if (err != ESP_OK) return err;
    drain_completions();
    err = esp_lcd_panel_init(panel);
    if (err == ESP_OK) err = wait_frame();
    if (err != ESP_OK) return err;
    ready = true;
    uint16_t *back = board_lcd_back_buffer();
    prepare(back);
    return board_lcd_publish(back);
}

esp_err_t board_lcd_recover(board_lcd_prepare_t prepare)
{
    if (!prepare || !frame_done) return ESP_ERR_INVALID_STATE;
    if (ready) return ESP_OK;
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= 3; ++attempt) {
        ESP_LOGW(TAG, "LCD recovery attempt %u/3", attempt);
        if (panel) {
            err = restart_panel(prepare);
            if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
                err = delete_panel();
                if (err == ESP_OK) err = create_panel(prepare);
            }
        } else err = create_panel(prepare);
        if (err == ESP_OK) { ESP_LOGI(TAG, "LCD recovery complete"); return ESP_OK; }
        ESP_LOGE(TAG, "LCD recovery attempt %u failed: %s", attempt, esp_err_to_name(err));
    }
    delete_panel();
    return err;
}
