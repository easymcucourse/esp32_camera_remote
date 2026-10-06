#pragma once
#include "camera_controls.h"
#include "camera_frames.h"
#include "camera_outputs.h"
/* Per-session execution owned by Camera, not another protocol/session layer.
 * Backend is borrowed from camera_session only for a synchronous tick. Caller
 * supplies the two existing object buffers and a monotonic clock; no worker,
 * additional object buffer, UI implementation or transport wrapper is added. */
typedef struct {
    camera_controls_t controls;
    camera_properties_t properties;
    setting_control_t mode;
    camera_menu_t menu;
    camera_frames_t frames;
    uint32_t generation, next_read, retry_at, safety_generation, properties_at;
    unsigned refused;
    bool running, refresh, publish;
    int mode_steps, menu_steps[CAMERA_MENU_COUNT];
    bool focus_pending;
    int focus_direction;
    uint32_t focus_at, focus_generation;
    atomic_uint focus_epoch;
    uint32_t focus_request_epoch;
} camera_stream_t;
/* Fresh zeroed storage, or ended storage after all leases and backend cleanup.
 * Keeps control guard callbacks and the safety sequence across sessions. */
bool camera_stream_begin(camera_stream_t *stream, uint32_t generation,
    uint8_t *first, uint8_t *second, size_t capacity, pad_lens_t lens, uint32_t now);
/* Owner intake; UI frame results remain accepted during stop/drain. Settings
 * requests use semantic property ID in command.index and ±1 direction. */
esp_err_t camera_stream_message(camera_stream_t *stream, const app_message_t *message, uint32_t now);
camera_backend_result_t camera_stream_tick(camera_stream_t *stream, camera_backend_t *backend,
    uint32_t timeout_ms, uint32_t (*now_ms)(void *context), void *clock_context);
/* Stop admission first, execute healthy-boundary safety releases with NULL
 * scratch, then drain frame leases before session cleanup/free/end. */
void camera_stream_stop(camera_stream_t *stream, uint32_t now);
camera_backend_result_t camera_stream_release(camera_stream_t *stream, camera_backend_t *backend,
    uint32_t timeout_ms, uint32_t (*now_ms)(void *context), void *clock_context);
bool camera_stream_end(camera_stream_t *stream);
/* Nonblocking intake cancellation; checked after property IO, before MF write. */
void camera_stream_focus_cancel(camera_stream_t *stream);
