#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define DISPLAY_SURFACE_API_VERSION 1

typedef struct {
    uint16_t *pixels;
    size_t width, height, stride_pixels;
    uint32_t generation;
    uintptr_t lease;
} display_canvas_t;

typedef struct {
    size_t width, height, stride_pixels;
    uint32_t generation;
    bool ready, acquired;
} display_surface_status_t;

/* Called only with buffers whose write ownership has been confirmed. The UI
 * initializes all pixels, including during panel recovery. Must not reenter. */
typedef void (*display_prepare_t)(uint16_t *pixels);

/* One-shot initialization from the startup task; failure cannot be retried. */
/* prepare_resources runs after power-up and before the first pixel drawing,
 * preserving hardware startup order (e.g. font initialization). */
esp_err_t display_surface_init(display_prepare_t prepare, esp_err_t (*prepare_resources)(void));
/* Task API (not ISR). One writer; timeout_ms is bounded, zero is nonblocking.
 * Keep the same canvas object alive and unmodified until refresh/cancel; copies
 * cannot submit or release it. Refresh/cancel on every exit, never retain pixels. */
esp_err_t display_canvas_acquire(display_canvas_t *canvas, uint32_t timeout_ms);
/* Success and failure consume the lease. A stale/copy lease cannot submit or
 * cancel another writer. Failed publication blocks writes until recovery. */
esp_err_t display_canvas_refresh(display_canvas_t *canvas);
void display_canvas_cancel(display_canvas_t *canvas);
/* Recovery refuses an active writer; every recovery invalidates old generations. */
esp_err_t display_surface_recover(void);
void display_surface_get_status(display_surface_status_t *status);
esp_err_t display_surface_test_fault(unsigned mode);
