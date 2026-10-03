#include "board_lcd.h"
#include "board_7b.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

struct fake_panel { uint16_t buffers[2][8]; bool prepared[2], started; } panels[32];
static struct fake_panel *active;
static esp_lcd_rgb_panel_event_callbacks_t callback;
static unsigned generation, creates, deletes, successful_deletes, completions, notices = 1000;
static unsigned create_failures, delete_failures, buffer_failures, prepare_calls, reset_failures;
static bool draw_failure;
void fake_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
const char *esp_err_to_name(esp_err_t err) { (void)err; return "fake"; }
SemaphoreHandle_t xSemaphoreCreateCounting(unsigned max, unsigned initial)
{ assert(max == 4 && initial == 0); return &completions; }
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t *wake)
{ assert(sem == &completions); if (completions < 4) ++completions; *wake = pdTRUE; return pdTRUE; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout)
{
    assert(sem == &completions);
    if (!completions && timeout) {
        assert(timeout == 1000);
        if (!notices) return pdFALSE;
        assert(active && active->started); --notices;
        callback.on_frame_buf_complete(active, NULL, NULL);
    }
    if (!completions) return pdFALSE;
    --completions; return pdTRUE;
}
esp_err_t esp_lcd_new_rgb_panel(const esp_lcd_rgb_panel_config_t *cfg, esp_lcd_panel_handle_t *out)
{
    assert(!active); ++creates;
    assert(cfg->num_fbs == 2 && cfg->timings.pclk_hz == 18000000 && cfg->bounce_buffer_size_px == 30720);
    if (create_failures) { --create_failures; return ESP_ERR_NO_MEM; }
    assert(generation < 32); active = &panels[generation++]; *out = active; return ESP_OK;
}
esp_err_t esp_lcd_panel_del(esp_lcd_panel_handle_t panel)
{
    assert(panel == active); ++deletes;
    if (delete_failures) { --delete_failures; return ESP_FAIL; }
    ++successful_deletes;
    /* A late completion while retiring the panel must not confirm a new frame. */
    completions = 4; active = NULL; return ESP_OK;
}
esp_err_t esp_lcd_rgb_panel_get_frame_buffer(esp_lcd_panel_handle_t panel, unsigned count, ...)
{
    assert(panel == active && count == 2);
    if (buffer_failures) { --buffer_failures; return ESP_FAIL; }
    va_list args; va_start(args, count);
    *va_arg(args, void **) = panel->buffers[0]; *va_arg(args, void **) = panel->buffers[1];
    va_end(args); return ESP_OK;
}
esp_err_t esp_lcd_rgb_panel_register_event_callbacks(esp_lcd_panel_handle_t panel,
    const esp_lcd_rgb_panel_event_callbacks_t *cb, void *ctx)
{ assert(panel == active && !ctx); callback = *cb; return ESP_OK; }
static void prepare(uint16_t *pixels)
{
    assert(active); ++prepare_calls;
    if (active->started) assert(board_lcd_ready() && pixels == board_lcd_back_buffer());
    unsigned index = pixels == active->buffers[1]; assert(pixels == active->buffers[index]);
    active->prepared[index] = true; pixels[0] = (uint16_t)generation;
}
esp_err_t esp_lcd_panel_reset(esp_lcd_panel_handle_t panel)
{
    assert(panel == active);
    if (reset_failures) { --reset_failures; return ESP_FAIL; }
    panel->started = false; return ESP_OK;
}
esp_err_t esp_lcd_panel_init(esp_lcd_panel_handle_t panel)
{ assert(panel == active && panel->prepared[0] && panel->prepared[1]); panel->started = true; return ESP_OK; }
esp_err_t esp_lcd_panel_draw_bitmap(esp_lcd_panel_handle_t panel, int x0, int y0, int x1, int y1, const void *pixels)
{
    assert(panel == active && x0 == 0 && y0 == 0 && x1 == 1024 && y1 == 600);
    assert(pixels == panel->buffers[0] || pixels == panel->buffers[1]);
    if (draw_failure) { draw_failure = false; return ESP_FAIL; }
    return ESP_OK;
}
int main(void)
{
    assert(board_lcd_init(prepare) == ESP_OK && prepare_calls == 2);
    uint16_t *back = board_lcd_back_buffer(); assert(back == active->buffers[1]);
    assert(board_lcd_publish(back) == ESP_OK && board_lcd_back_buffer() == active->buffers[0]);
    assert(board_lcd_publish(NULL) == ESP_ERR_INVALID_ARG && board_lcd_ready());
    completions = 4; notices = 0; /* Stale ISR tokens cannot mask lost synchronization. */
    assert(board_lcd_publish(board_lcd_back_buffer()) == ESP_ERR_TIMEOUT);
    assert(!board_lcd_ready() && !board_lcd_back_buffer());
    assert(board_lcd_publish(back) == ESP_ERR_INVALID_STATE);
    delete_failures = 1; buffer_failures = 1; reset_failures = 2; notices = 1000;
    unsigned before = creates;
    assert(board_lcd_recover(prepare) == ESP_OK && creates == before + 2);
    assert(board_lcd_back_buffer() == active->buffers[1] && back != board_lcd_back_buffer());
    before = creates; assert(board_lcd_recover(prepare) == ESP_OK && creates == before);
    draw_failure = true; assert(board_lcd_publish(board_lcd_back_buffer()) == ESP_FAIL);
    create_failures = 3; reset_failures = 1; before = creates;
    assert(board_lcd_recover(prepare) == ESP_ERR_NO_MEM && creates == before + 3);
    assert(!active && !board_lcd_ready() && !board_lcd_back_buffer());
    notices = 0; before = creates;
    assert(board_lcd_recover(prepare) == ESP_ERR_TIMEOUT && creates == before + 1);
    assert(!active && !board_lcd_back_buffer());
    notices = 1000; assert(board_lcd_recover(prepare) == ESP_OK);
    assert(board_lcd_test_fault(3) == ESP_ERR_INVALID_ARG);
    assert(board_lcd_test_fault(1) == ESP_OK);
    assert(board_lcd_publish(board_lcd_back_buffer()) == ESP_ERR_TIMEOUT);
    assert(board_lcd_recover(prepare) == ESP_OK); /* One-shot ends on scan restart. */
    assert(board_lcd_test_fault(2) == ESP_OK);
    assert(board_lcd_publish(board_lcd_back_buffer()) == ESP_ERR_TIMEOUT);
    assert(board_lcd_recover(prepare) == ESP_ERR_TIMEOUT && !board_lcd_back_buffer());
    assert(board_lcd_test_fault(0) == ESP_OK && board_lcd_recover(prepare) == ESP_OK);
    assert(successful_deletes == generation - 1 && deletes == successful_deletes + 1);
    puts("LCD framebuffer ownership, stale callbacks, partial setup and bounded recovery passed");
}
