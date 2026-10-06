#pragma once
#include "app_core_factory.h"

typedef struct {
    factory_reset_ops_t transaction;
    bool (*freeze_config)(void *context, unsigned timeout_ms);
    void (*resume_config)(void *context);
    void (*get_config)(void *context, network_config_t *config);
    uint32_t (*next_token)(void); /* Globally unique, task-safe/nonblocking. */
    void *context;
} app_core_factory_ops_t;
/* Composition start once, copied callback table. Worker serializes full reset,
 * freezes configuration before taking a fresh snapshot and draining camera,
 * then reboots after successful completion. Failure resumes config admission.
 * Requests are copied/nonblocking; result history retains eight tokens. */
esp_err_t app_core_factory_start(const app_core_factory_ops_t *ops);
esp_err_t app_core_factory_request(uint32_t *token);
esp_err_t app_core_factory_result(uint32_t token, esp_err_t *result);
/* Core-only, before Camera/UI/config shutdown. Close admission, cancel queued
 * jobs and let an active persistence transaction finish (success reboots).
 * Timeout retains worker/queue and keeps admission closed; retry is allowed.
 * Repeated completed stop succeeds; start may then create a fresh worker. */
bool app_core_factory_quiesce(uint32_t timeout_ms);
