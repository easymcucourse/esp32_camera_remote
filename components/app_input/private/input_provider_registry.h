#pragma once
#include "input_provider.h"
#define INPUT_PROVIDER_REPORT_CAPACITY 16
typedef struct {
    input_provider_handle_t handle;
    input_source_kind_t source;
    input_report_t report;
    uint32_t captured_ms;
    bool disconnected;
    input_disconnect_reason_t reason;
} input_provider_event_t;
/* Lifecycle/consumer are private to input_service. No task or device ownership
 * is introduced here. Stop closes admission before draining safety events. */
esp_err_t input_provider_registry_init(void);
esp_err_t input_provider_registry_deinit(void);
void input_provider_registry_close(void);
bool input_provider_registry_next(input_provider_event_t *event);
bool input_provider_registry_idle(void);
