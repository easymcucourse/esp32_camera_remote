#pragma once
#include "gamepad_input.h"
#include <stddef.h>

#define CAMERA_ACTION_CAPACITY 32
typedef struct { pad_action_t action; uint32_t queued_ms; bool release; } camera_action_t;
typedef struct {
    camera_action_t entries[CAMERA_ACTION_CAPACITY];
    size_t head, count;
    uint32_t generation;
    bool session, release_pending, s1, s2;
    int zoom;
} camera_actions_t;
/* Caller synchronizes all access. Owner marks potentially latched controls
 * before network I/O, so an offline race still schedules their release. */
void camera_actions_session(camera_actions_t *q, bool open);
bool camera_actions_submit(camera_actions_t *q, pad_action_t action, uint32_t now_ms);
bool camera_actions_next(camera_actions_t *q, uint32_t now_ms, camera_action_t *out);
bool camera_actions_current(const camera_actions_t *q, const camera_action_t *action);
bool camera_actions_record_queued(const camera_actions_t *q);
/* Release latches clear only after a successful write, never when popped. */
void camera_actions_complete(camera_actions_t *q, const camera_action_t *action, bool accepted);
