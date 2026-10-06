#pragma once
#include "app_console.h"
#include <stdatomic.h>
#define CAMERA_FRAME_SLOTS 2
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    atomic_bool returned;
    bool acquired, awaiting, completed;
    uint32_t token;
    esp_err_t result;
} camera_frame_slot_t;
typedef struct {
    camera_frame_slot_t slots[CAMERA_FRAME_SLOTS];
    uint32_t generation, next_token, next_result;
    unsigned shown, dropped, bad_streak;
    bool failed;
} camera_frames_t;
/* Sole producer owns state and buffers. Only returned is touched by another
 * task, from the last lease destructor. Slot reuse waits for both ownership
 * return and typed UI result. Metadata results are applied in frame order. */
bool camera_frames_init(camera_frames_t *frames, uint32_t generation,
    uint8_t *first, uint8_t *second, size_t capacity);
int camera_frames_acquire(camera_frames_t *frames);
void camera_frames_discard(camera_frames_t *frames, int slot);
/* Backend rejected an envelope before JPEG delivery. Count damage in the same
 * sequence as UI results without publishing protocol bytes to UI. */
bool camera_frames_drop(camera_frames_t *frames, int slot);
esp_err_t camera_frames_publish(camera_frames_t *frames, int slot,
    const uint8_t *jpeg, size_t length, uint32_t read_ms);
bool camera_frames_result(camera_frames_t *frames, const app_message_t *message);
/* Does not wait for result metadata: all consumer references must return before
 * buffers/context can be freed or reused for cleanup protocol traffic. */
bool camera_frames_drained(const camera_frames_t *frames);
