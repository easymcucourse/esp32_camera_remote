#pragma once
#include "app_wifi.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#define APP_MAINTENANCE_API_VERSION 1

typedef enum {
    APP_MAINTENANCE_IDLE,
    APP_MAINTENANCE_TRIGGER,
    APP_MAINTENANCE_ACTIVATING,
    APP_MAINTENANCE_ACTIVE,
    APP_MAINTENANCE_CLOSED,
} app_maintenance_phase_t;
typedef struct {
    app_maintenance_phase_t phase;
    bool initialized;
} app_maintenance_status_t;
typedef struct {
    /* Called on the existing HTTP task, without a functional message bus.
     * Core must finish draining and call activate before returning true.
     * If Core acquired exclusivity and then failed, it must schedule reboot;
     * false may also mean normal mode won, which must not reboot that mode.
     * Core must never stop this HTTP task while this callback is waiting. */
    bool (*request_exclusive)(void *context, uint32_t timeout_ms);
    void (*request_reboot)(void *context);
    void *context;
} app_maintenance_system_ops_t;

/* Single Core initialization, after Web/OTA ops are initialized for the same
 * Wi-Fi object. No task or queue is created. Publication uses the original
 * Web HTTP task and its original stack/priority/socket limits.
 * Lifecycle never returns to IDLE after close/activation/failure. */
/* Core serializes lifecycle calls. Repeated successful init/open/activate
 * returns INVALID_STATE; rejected init leaves initialization retryable.
 * Callback tables are copied; contexts must remain valid through HTTP stop.
 * get_status is a task-safe value snapshot, not an admission reservation. */
esp_err_t app_maintenance_init(app_wifi_t *wifi,const app_maintenance_system_ops_t *system);
esp_err_t app_maintenance_trigger_open(void);
/* Irreversibly close trigger admission; external owner calls stop below to
 * physically remove TCP:80 before approving a normal UI transition. */
void app_maintenance_trigger_close(void);
/* Core only, after every normal owner has stopped and the fixed LCD is ready.
 * Switches the existing HTTP task's gate, never starts another server. */
esp_err_t app_maintenance_activate(void);
void app_maintenance_get_status(app_maintenance_status_t *status);
/* External Core task only. Wakes clients and joins HTTP. Failed stop keeps the
 * server owner and closed gate; caller must reboot rather than resume. */
esp_err_t app_maintenance_stop(void);
