#include "display_surface.h"
#include "display_backend.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct { unsigned count; } semaphores[2];
static unsigned created, publications, preparations, recoveries, front;
static uint16_t pixels[2][32];
static bool ready;
static esp_err_t publish_result = ESP_OK, recover_result = ESP_OK, init_result = ESP_OK;
static TickType_t ticks;
static bool starve_final_lock;
TickType_t xTaskGetTickCount(void) { return ticks; }

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{ assert(created == 0); semaphores[created].count = 1; return &semaphores[created++]; }
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{ assert(created == 1); return &semaphores[created++]; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t timeout)
{
    (void)timeout;
    for (unsigned i = 0; i < 2; ++i) if (handle == &semaphores[i]) {
        if (!semaphores[i].count) return pdFALSE;
        --semaphores[i].count;
        if (i == 1 && starve_final_lock) { semaphores[0].count = 0; starve_final_lock = false; }
        return pdTRUE;
    }
    assert(false); return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{
    for (unsigned i = 0; i < 2; ++i) if (handle == &semaphores[i]) {
        assert(!semaphores[i].count); ++semaphores[i].count; return pdTRUE;
    }
    assert(false); return pdFALSE;
}
static void prepare(uint16_t *buffer) { ++preparations; memset(buffer, 0, 64); }
static unsigned resources_initialized;
static esp_err_t resources_init(void) { ++resources_initialized; return ESP_OK; }
esp_err_t display_backend_init(display_backend_prepare_t callback, esp_err_t (*resources)(void))
{
    assert(!resources_initialized);
    if (resources) assert(resources() == ESP_OK && resources_initialized == 1);
    callback(pixels[0]); callback(pixels[1]); ready = init_result == ESP_OK; return init_result;
}
bool display_backend_ready(void) { return ready; }
void display_backend_dimensions(size_t *width, size_t *height, size_t *stride)
{ *width = 4; *height = 4; *stride = 8; }
uint16_t *display_backend_back_buffer(void) { return ready ? pixels[front ^ 1] : NULL; }
esp_err_t display_backend_publish(uint16_t *buffer)
{
    assert(ready && buffer == pixels[front ^ 1]); ++publications;
    if (publish_result == ESP_OK) front ^= 1;
    else ready = false;
    return publish_result;
}
esp_err_t display_backend_recover(display_backend_prepare_t callback)
{
    assert(!ready); ++recoveries;
    if (recover_result == ESP_OK) { callback(pixels[front ^ 1]); ready = true; }
    return recover_result;
}
esp_err_t display_backend_test_fault(unsigned mode)
{ return mode <= 2 ? ESP_OK : ESP_ERR_INVALID_ARG; }

int main(int argc, char **argv)
{
    (void)argv;
    display_canvas_t canvas = {0}, other = {0};
    display_surface_status_t status;
    display_surface_get_status(&status); assert(!status.ready && !status.acquired);
    assert(display_canvas_acquire(&canvas, 0) == ESP_ERR_INVALID_STATE);
    assert(display_surface_init(NULL, NULL) == ESP_ERR_INVALID_ARG);
    if (argc > 1) {
        init_result = ESP_ERR_NO_MEM;
        assert(display_surface_init(prepare, resources_init) == ESP_ERR_NO_MEM);
        assert(display_surface_init(prepare, resources_init) == ESP_ERR_INVALID_STATE);
        assert(display_surface_recover() == ESP_ERR_INVALID_STATE);
        assert(display_canvas_acquire(&canvas, 0) == ESP_ERR_INVALID_STATE);
        puts("partial init cannot acquire or retry"); return 0;
    }
    assert(display_surface_init(prepare, resources_init) == ESP_OK && preparations == 2);
    assert(display_surface_init(prepare, resources_init) == ESP_ERR_INVALID_STATE);
    semaphores[0].count = 0;
    assert(display_canvas_acquire(&canvas, 25) == ESP_ERR_TIMEOUT);
    semaphores[0].count = 1;
    starve_final_lock = true;
    assert(display_canvas_acquire(&canvas, 25) == ESP_ERR_TIMEOUT);
    semaphores[0].count = 1;
    assert(display_canvas_acquire(&canvas, 0) == ESP_OK);
    assert(canvas.pixels == pixels[1] && canvas.width == 4 && canvas.height == 4 && canvas.stride_pixels == 8);
    assert(display_canvas_acquire(&canvas, 0) == ESP_ERR_INVALID_STATE && canvas.pixels);
    assert(display_canvas_acquire(&other, 25) == ESP_ERR_TIMEOUT);
    assert(display_surface_recover() == ESP_ERR_INVALID_STATE);
    display_surface_get_status(&status); assert(status.ready && status.acquired);
    display_canvas_t stale = canvas;
    assert(display_canvas_refresh(&stale) == ESP_ERR_INVALID_STATE && !stale.pixels);
    assert(!publications); /* A copied lease is not the owner. */
    canvas.pixels[0] = 0xf800; /* A partial decode must not publish. */
    stale = canvas;
    display_canvas_cancel(&canvas); assert(!canvas.pixels && !publications);
    assert(display_canvas_acquire(&canvas, 0) == ESP_OK && canvas.generation != stale.generation);
    display_canvas_cancel(&stale);
    assert(display_canvas_acquire(&other, 0) == ESP_ERR_TIMEOUT);
    assert(display_canvas_refresh(&canvas) == ESP_OK && !canvas.pixels && publications == 1);
    assert(display_canvas_acquire(&canvas, 0) == ESP_OK && canvas.pixels == pixels[0]);
    stale = canvas;
    publish_result = ESP_ERR_TIMEOUT;
    assert(display_canvas_refresh(&canvas) == ESP_ERR_TIMEOUT && !canvas.pixels);
    display_surface_get_status(&status); assert(!status.ready && !status.acquired);
    assert(display_canvas_acquire(&canvas, 0) == ESP_ERR_INVALID_STATE);
    recover_result = ESP_FAIL;
    assert(display_surface_recover() == ESP_FAIL);
    assert(display_canvas_acquire(&canvas, 0) == ESP_ERR_INVALID_STATE);
    recover_result = ESP_OK;
    assert(display_surface_recover() == ESP_OK && recoveries == 2);
    assert(display_surface_recover() == ESP_OK && recoveries == 2);
    assert(display_canvas_acquire(&canvas, 0) == ESP_OK && canvas.generation != stale.generation);
    assert(display_canvas_refresh(&stale) == ESP_ERR_INVALID_STATE);
    publish_result = ESP_OK;
    assert(display_canvas_refresh(&canvas) == ESP_OK && publications == 3);
    assert(display_surface_test_fault(3) == ESP_ERR_INVALID_ARG);
    assert(display_surface_test_fault(1) == ESP_OK);
    display_canvas_cancel(&canvas);
    puts("display lease, cancellation, failure and recovery passed");
    return 0;
}
