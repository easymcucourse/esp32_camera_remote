#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#define APP_CAMERA_API_VERSION 1u
/* Core validates the linked lifecycle contract before creating an owner. */
uint32_t app_camera_api_version(void);
typedef enum { APP_CAMERA_BACKEND_DEFAULT } app_camera_backend_t;
typedef struct { app_camera_backend_t backend; } app_camera_config_t;
typedef enum { APP_CAMERA_START_PREVIEW, APP_CAMERA_START_PAIR } app_camera_start_mode_t;
/* Core lifecycle only. Functional callers use Camera messages; this API never
 * exposes a backend, protocol object, task, buffer or mutable snapshot. */
/* Single Core startup caller. Init allocates private queues; repeated init is
 * INVALID_STATE. On later creation failure call stop then messages_quiesce to
 * release allocations, including when no endpoint was successfully started. */
esp_err_t app_camera_init(const app_camera_config_t *config);
esp_err_t app_camera_messages_start(void);
esp_err_t app_camera_start(app_camera_start_mode_t mode);
/* Core exclusive shutdown. Close new starts/ordinary controls immediately but
 * allow safety releases and completion metadata. stop then cancels the physical
 * owner and waits for channels/JPEG leases. Timeout keeps the gate closed;
 * successful repeats succeed. No release/reset until a device reboot. */
void app_camera_close_admission(void);
bool app_camera_stop(uint32_t timeout_ms);
/* Core only after successful physical quiesce, with all input/bench/factory
 * producers stopped. Retire/join endpoint and release result metadata queue.
 * Timeout retains worker/queue; retry is allowed. Completed repeats succeed. */
bool app_camera_messages_quiesce(uint32_t timeout_ms);
