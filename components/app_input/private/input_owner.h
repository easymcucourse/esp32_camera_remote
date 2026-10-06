#pragma once
#include "input_reports.h"
#include "input_provider_registry.h"
typedef struct {
    input_reports_t reports;
    input_provider_handle_t handles[INPUT_SOURCE_COUNT];
    input_report_t latest;
    input_source_kind_t selected;
} input_owner_t;
void input_owner_init(input_owner_t *owner, pad_action_fn emit, void *context);
bool input_owner_select(input_owner_t *owner, input_source_kind_t source);
/* The sole input_service consumer invokes this after draining capability/UI
 * messages. No provider interprets capabilities or business actions. */
void input_owner_tick(input_owner_t *owner, const gamepad_caps_t *caps, uint32_t now_ms);
bool input_owner_quiesce(input_owner_t *owner);
