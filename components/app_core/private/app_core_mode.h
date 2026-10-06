#pragma once
#include <stdbool.h>
#include <stdatomic.h>
typedef enum { APP_CORE_STARTUP,APP_CORE_NORMAL,APP_CORE_ACTIVATING,APP_CORE_MAINTENANCE,APP_CORE_RESTART } app_core_mode_value_t;
typedef struct { atomic_uint state; atomic_bool booting; } app_core_mode_t;
#define APP_CORE_MODE_INITIALIZER { APP_CORE_STARTUP, false }
/* Startup owner brackets resource creation. A claim may change mode meanwhile,
 * but exclusive close/drain waits for this release before touching owners. */
void app_core_mode_boot_begin(app_core_mode_t *mode);
void app_core_mode_boot_end(app_core_mode_t *mode);
bool app_core_mode_booting(const app_core_mode_t *mode);
/* Core's one mode owner; only a reboot creates a new STARTUP lifetime. */
extern app_core_mode_t app_core_mode;
app_core_mode_value_t app_core_mode_get(const app_core_mode_t *mode);
bool app_core_mode_enter_normal(app_core_mode_t *mode);
bool app_core_mode_request_maintenance(app_core_mode_t *mode);
/* Called only after Core confirms all physical owners/leases have drained. */
bool app_core_mode_activate(app_core_mode_t *mode);
void app_core_mode_restart(app_core_mode_t *mode);
