#pragma once
#include "gamepad_input.h"

/* Owner-only report kernel. Providers never access this state. The public
 * provider registry serializes copied reports; the message service must route
 * actions through Console. */
typedef enum { INPUT_REPORT_NONE, INPUT_REPORT_ATOM, INPUT_REPORT_SIM,
    INPUT_REPORT_SOURCE_COUNT } input_report_source_t;
typedef struct {
    gamepad_snapshot_t snapshot;
    uint32_t source_epoch, report_id;
    bool gap, event_valid;
    uint32_t event_buttons;
} input_report_frame_t;
typedef struct { uint32_t epoch, id; bool known, quarantined; } input_report_cursor_t;
typedef struct {
    gamepad_input_t gamepad;
    pad_action_fn emit;
    void *context;
    input_report_source_t selected;
    input_report_cursor_t sources[INPUT_REPORT_SOURCE_COUNT];
    bool release_pending, mf_pending, baseline;
} input_reports_t;

/* A true sink result acknowledges ownership of a safety action. A failed
 * safety handoff blocks presses until retried successfully. Hardware execution
 * acknowledgement belongs to input_service and the Camera endpoint, not this pure kernel.
 * Epochs and IDs are positive and do not wrap inside a provider lifetime.
 * An epoch advance permits ID reset; an ID rollback quarantines that epoch. */
void input_reports_init(input_reports_t *state, pad_action_fn emit, void *context);
bool input_reports_select(input_reports_t *state, input_report_source_t source);
bool input_reports_retry_release(input_reports_t *state);
void input_reports_disconnect(input_reports_t *state, input_report_source_t source);
bool input_reports_publish(input_reports_t *state, input_report_source_t source,
    const input_report_frame_t *report, const gamepad_caps_t *caps, uint32_t now_ms);
