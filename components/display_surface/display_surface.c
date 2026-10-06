#include "display_surface.h"
#include "display_backend.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

static SemaphoreHandle_t mutex, available;
static display_prepare_t prepare_buffers;
static display_canvas_t active;
static display_canvas_t *active_owner;
static uintptr_t next_lease;
static uint32_t generation = 1;
static bool initialized;

esp_err_t display_surface_init(display_prepare_t prepare, esp_err_t (*prepare_resources)(void))
{
    if (!prepare) return ESP_ERR_INVALID_ARG;
    if (mutex || available) return ESP_ERR_INVALID_STATE;
    mutex = xSemaphoreCreateMutex();
    available = xSemaphoreCreateBinary();
    if (!mutex || !available) return ESP_ERR_NO_MEM;
    prepare_buffers = prepare;
    esp_err_t err = display_backend_init(prepare, prepare_resources);
    if (err == ESP_OK) { initialized = true; xSemaphoreGive(available); }
    return err;
}

static bool matches(const display_canvas_t *canvas)
{
    return canvas && canvas == active_owner && active.lease && canvas->lease == active.lease &&
        canvas->generation == active.generation && canvas->pixels == active.pixels &&
        canvas->width == active.width && canvas->height == active.height &&
        canvas->stride_pixels == active.stride_pixels;
}

static TickType_t remaining(TickType_t started, TickType_t budget)
{
    TickType_t elapsed = xTaskGetTickCount() - started;
    return elapsed < budget ? budget - elapsed : 0;
}

esp_err_t display_canvas_acquire(display_canvas_t *canvas, uint32_t timeout_ms)
{
    if (!canvas) return ESP_ERR_INVALID_ARG;
    if (!initialized) return ESP_ERR_INVALID_STATE;
    TickType_t budget = pdMS_TO_TICKS(timeout_ms);
    if (timeout_ms && !budget) budget = 1;
    if (budget == portMAX_DELAY) --budget;
    TickType_t started = xTaskGetTickCount();
    /* Do not overwrite a live lease if the same object is acquired twice. */
    if (xSemaphoreTake(mutex, budget) != pdTRUE) return ESP_ERR_TIMEOUT;
    if (canvas == active_owner) { xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE; }
    bool ready = display_backend_ready();
    xSemaphoreGive(mutex);
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(available, remaining(started, budget)) != pdTRUE) return ESP_ERR_TIMEOUT;
    if (xSemaphoreTake(mutex, remaining(started, budget)) != pdTRUE) {
        xSemaphoreGive(available); return ESP_ERR_TIMEOUT;
    }
    uint16_t *pixels = display_backend_back_buffer();
    if (!pixels) {
        xSemaphoreGive(mutex); xSemaphoreGive(available);
        return ESP_ERR_INVALID_STATE;
    }
    if (++next_lease == 0) ++next_lease;
    active = (display_canvas_t){.pixels = pixels, .generation = generation, .lease = next_lease};
    display_backend_dimensions(&active.width, &active.height, &active.stride_pixels);
    *canvas = active;
    active_owner = canvas;
    xSemaphoreGive(mutex);
    return ESP_OK;
}

esp_err_t display_canvas_refresh(display_canvas_t *canvas)
{
    if (!canvas) return ESP_ERR_INVALID_ARG;
    if (!mutex) { memset(canvas, 0, sizeof(*canvas)); return ESP_ERR_INVALID_STATE; }
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!matches(canvas)) {
        memset(canvas, 0, sizeof(*canvas));
        xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = display_backend_publish(active.pixels);
    memset(&active, 0, sizeof(active));
    active_owner = NULL;
    memset(canvas, 0, sizeof(*canvas));
    ++generation;
    xSemaphoreGive(mutex); xSemaphoreGive(available);
    return err;
}

void display_canvas_cancel(display_canvas_t *canvas)
{
    if (!canvas) return;
    if (!mutex) { memset(canvas, 0, sizeof(*canvas)); return; }
    xSemaphoreTake(mutex, portMAX_DELAY);
    bool owned = matches(canvas);
    if (owned) { memset(&active, 0, sizeof(active)); active_owner = NULL; ++generation; }
    memset(canvas, 0, sizeof(*canvas));
    xSemaphoreGive(mutex);
    if (owned) xSemaphoreGive(available);
}

esp_err_t display_surface_recover(void)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (active.lease) { xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE; }
    esp_err_t err = ESP_OK;
    if (!display_backend_ready()) {
        ++generation;
        err = display_backend_recover(prepare_buffers);
    }
    xSemaphoreGive(mutex);
    return err;
}

void display_surface_get_status(display_surface_status_t *status)
{
    if (!status) return;
    memset(status, 0, sizeof(*status));
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    display_backend_dimensions(&status->width, &status->height, &status->stride_pixels);
    status->generation = generation;
    status->ready = display_backend_ready();
    status->acquired = active.lease != 0;
    xSemaphoreGive(mutex);
}

esp_err_t display_surface_test_fault(unsigned mode)
{
    if (!initialized) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    esp_err_t err = display_backend_test_fault(mode);
    xSemaphoreGive(mutex);
    return err;
}
