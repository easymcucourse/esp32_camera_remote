#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sony_props.h"

/* Socket-owner-only target merging. Input tasks hand off step counts separately.
 * No optimistic actual value: only a valid camera snapshot confirms APPLIED. */
typedef enum {
    SETTING_IDLE, SETTING_PENDING, SETTING_APPLIED, SETTING_REJECTED, SETTING_TIMEOUT, SETTING_ACCEPTED
} setting_status_t;
typedef struct {
    sony_mode_state_t snapshot;
    uint32_t desired, sent, deadline_ms;
    bool dirty, awaiting;
    setting_status_t status;
} setting_control_t;
void setting_control_snapshot(setting_control_t *s, const sony_mode_state_t *snapshot, uint32_t now_ms);
bool setting_control_step(setting_control_t *s, int steps);
bool setting_control_adjust(setting_control_t *s, int steps, bool wrap);
bool setting_control_next(setting_control_t *s, uint32_t now_ms, uint32_t *value);
void setting_control_response(setting_control_t *s, bool accepted);
