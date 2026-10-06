#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "pad_types.h"

/* Nonblocking handoff; false requests safety cancellation and input rearming. */
typedef bool (*pad_action_fn)(void *context, pad_action_t action);
typedef struct {
    pad_action_fn emit;
    void *context;
    gamepad_caps_t caps;
    uint32_t event_buttons, held_shoulder, repeat_ms;
    uint32_t held_direction, direction_repeat_ms;
    uint32_t select_deadline;
    bool select_hold;
    uint8_t rt_stage, shoulder_mode;
    bool lt_full; /* Recording edge only; LT has no half-press/focus state. */
    bool connected, trigger_armed, shoulder_armed, direction_armed, s1, s2;
} gamepad_input_t;
void gamepad_input_init(gamepad_input_t *s, pad_action_fn emit, void *context);
void gamepad_input_online(gamepad_input_t *s, const gamepad_snapshot_t *first,
                          const gamepad_caps_t *caps);
void gamepad_input_offline(gamepad_input_t *s);
void gamepad_input_event(gamepad_input_t *s, uint32_t buttons, bool gap,
                         const gamepad_caps_t *caps, uint32_t now_ms);
void gamepad_input_snapshot(gamepad_input_t *s, const gamepad_snapshot_t *snapshot,
                            const gamepad_caps_t *caps, uint32_t now_ms);
