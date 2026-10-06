#pragma once
#include "app_core.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct app_wifi app_wifi_t;

/* Core copies health callbacks; no retained caller stack pointer. health_tick
 * is nonblocking; shutdown and exclusive switch run on the internal stack. */
typedef struct {
    void (*health_tick)(void);
    bool (*maintenance_quiesce)(unsigned timeout_ms);
    bool (*camera_drain)(uint32_t timeout_ms);
} app_core_health_ops_t;

/* Startup-task only, before publishing the boot resource barrier. Copies ops, starts
 * the existing 4096-byte/priority-2 internal-RAM health task exactly once.
 * Partial creation failure returns NO_MEM and permits retry. */
esp_err_t app_core_health_start(const app_core_health_ops_t *ops);
/* Begins router/System/UI endpoints before normal inputs and Camera. */
esp_err_t app_core_messages_start(void);
/* Startup owner binds the Wi-Fi object once before normal service startup.
 * Checks version/AP/store/config/TCP capabilities; object outlives Core. */
esp_err_t app_core_network_bind(app_wifi_t *wifi);
esp_err_t app_core_input_providers_start(void);
/* Physical I2C device only; no normal tasks/endpoints before the HTTP trigger. */
esp_err_t app_core_input_providers_prepare(void);
esp_err_t app_core_input_providers_stop(uint32_t timeout_ms);
/* Core composes Camera and UART, never an old maintenance policy callback. */
esp_err_t app_core_camera_boot(void);
bool app_core_camera_quiesce(uint32_t timeout_ms);
