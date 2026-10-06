#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "network_config.h"

#define APP_WIFI_API_VERSION 1
#define APP_WIFI_CLIENT_CAPACITY 4
enum { APP_WIFI_CAP_AP = 1u << 0, APP_WIFI_CAP_CONFIG_STORE = 1u << 1,
    APP_WIFI_CAP_CONFIG_ASYNC = 1u << 2, APP_WIFI_CAP_TCP = 1u << 3 };
typedef enum {
    APP_WIFI_OK, APP_WIFI_INVALID, APP_WIFI_STATE, APP_WIFI_NO_MEMORY,
    APP_WIFI_TIMEOUT, APP_WIFI_CANCELLED, APP_WIFI_IO, APP_WIFI_UNSUPPORTED,
    APP_WIFI_PENDING, APP_WIFI_NOT_FOUND, APP_WIFI_STALE, APP_WIFI_CLOSED
} app_wifi_result_t;
typedef struct app_wifi app_wifi_t;
typedef struct { uint8_t mac[6]; char ip[16]; int32_t rssi; } app_wifi_client_t;
typedef struct {
    bool started, online;
    uint32_t generation;
    unsigned max_channel;
    char address[16];
    int32_t diagnostic; /* Implementation error, never interpreted by callers. */
} app_wifi_status_t;

/* One composition owner serializes lifecycle calls. Snapshots are task-safe.
 * Creation initializes the object; duplicate start and reconfigure before start
 * return STATE. Failed stop retains ownership for a retry. Destroy requires a
 * stopped object and clears the caller's handle only on success. */
unsigned app_wifi_api_version(const app_wifi_t *wifi);
uint32_t app_wifi_capabilities(const app_wifi_t *wifi);
/* Factory binding does not initialize radio hardware. Init is performed at the
 * original Wi-Fi startup point; start initializes automatically if necessary.
 * Duplicate init returns STATE. A failed init remains destroyable/retryable. */
app_wifi_result_t app_wifi_init(app_wifi_t *wifi);
/* Synchronous persistence primitives for composition/reset and backend apply.
 * Read restores corrupt records to defaults, without erasing unrelated NVS.
 * Write changes only wifi_ap/cfg, never restarts the network or publishes active
 * config. These operations may run before radio initialization. Async runtime
 * apply must serialize them with its prepare/commit/cancel/result worker. */
app_wifi_result_t app_wifi_saved_config_read(app_wifi_t *wifi, network_config_t *config);
app_wifi_result_t app_wifi_saved_config_write(app_wifi_t *wifi, const network_config_t *config);
app_wifi_result_t app_wifi_start(app_wifi_t *wifi, const network_config_t *config);
app_wifi_result_t app_wifi_reconfigure(app_wifi_t *wifi, const network_config_t *config);
app_wifi_result_t app_wifi_stop(app_wifi_t *wifi, uint32_t timeout_ms);
app_wifi_result_t app_wifi_destroy(app_wifi_t **wifi);
app_wifi_result_t app_wifi_get_status(app_wifi_t *wifi, app_wifi_status_t *status);
/* Only associated peers with DHCP leases. On insufficient capacity no partial
 * snapshot is returned. Neither caller nor status exposes SDK network objects. */
app_wifi_result_t app_wifi_get_clients(app_wifi_t *wifi, app_wifi_client_t *clients,
    size_t capacity, size_t *count);
/* Copied nonblocking requests. Prepare reserves without persistence/driver
 * changes; commit releases once after <=10s delay, cancel before commit only.
 * Uncommitted reservations expire after25s. Result history is bounded to8;
 * PENDING differs from expired/unknown NOT_FOUND. Snapshot is active config.
 * Freeze closes admission and waits existing transactions; timeout restores
 * admission. One composition owner freezes/resumes and controls lifecycle. */
uint32_t app_wifi_next_token(void);
app_wifi_result_t app_wifi_config_get(app_wifi_t *wifi, network_config_t *config);
app_wifi_result_t app_wifi_config_apply(app_wifi_t *wifi, const network_config_t *config, bool staged, uint32_t *token);
app_wifi_result_t app_wifi_config_commit(app_wifi_t *wifi, uint32_t token, unsigned delay_ms);
app_wifi_result_t app_wifi_config_cancel(app_wifi_t *wifi, uint32_t token);
app_wifi_result_t app_wifi_config_result(app_wifi_t *wifi, uint32_t token, app_wifi_result_t *result);
app_wifi_result_t app_wifi_config_freeze(app_wifi_t *wifi, uint32_t timeout_ms);
void app_wifi_config_resume(app_wifi_t *wifi);
/* Composition owner, after all normal config producers/factory transactions
 * stopped. Permanently close and join the normal config worker, cancel queued
 * jobs, finish an active apply/rollback. Timeout retains resources and closed
 * admission; retry is allowed. Leaves AP, snapshots, token history and saved
 * storage available for isolated maintenance. Optional backend operation. */
app_wifi_result_t app_wifi_config_quiesce(app_wifi_t *wifi, uint32_t timeout_ms);
/* Core owner only after successful config_quiesce and normal producers/router
 * stopped. Restart the same persistent-config worker for isolated maintenance;
 * no AP radio restart, no message bridge or TCP admission change, history and
 * current config retained. Duplicate/unfinished-stop returns STATE. Failed
 * creation leaves config admission closed and retryable. Optional backend op. */
app_wifi_result_t app_wifi_config_start(app_wifi_t *wifi);

typedef struct app_wifi_channel app_wifi_channel_t;
typedef struct { char address[16]; uint16_t port; } app_wifi_endpoint_t;
typedef struct {
    int64_t at_us; /* Absolute monotonic deadline, positive; never extended. */
    uint32_t generation; /* Zero captures current; nonzero rejects old network. */
    bool (*cancelled)(void *context); /* Task-safe, nonblocking. */
    void *context;
} app_wifi_deadline_t;
/* Synchronous exact-length I/O, zero copies. Buffer remains caller-owned until
 * return; bytes reports partial progress on errors. One owner submits I/O per
 * channel; overlapping I/O/close is rejected without releasing the handle.
 * cancel is task-safe while an operation runs and permanently cancels channel.
 * Stop/network change invalidate old generation; callers must close all handles
 * before destroying Wi-Fi. Close cancels first and returns STATE if I/O is
 * active; retry after it returns. Success consumes and clears the handle. */
app_wifi_result_t app_wifi_channel_connect(app_wifi_t *wifi, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, app_wifi_channel_t **channel);
app_wifi_result_t app_wifi_channel_send(app_wifi_channel_t *channel, const void *data, size_t size,
    const app_wifi_deadline_t *deadline, size_t *bytes);
app_wifi_result_t app_wifi_channel_receive(app_wifi_channel_t *channel, void *data, size_t size,
    const app_wifi_deadline_t *deadline, size_t *bytes);
void app_wifi_channel_cancel(app_wifi_channel_t *channel);
/* Nonblocking readiness snapshot, consumes no bytes. Optional driver op;
 * absent support returns UNSUPPORTED. Same single-owner/deadline rules as I/O. */
app_wifi_result_t app_wifi_channel_poll(app_wifi_channel_t *channel,
    const app_wifi_deadline_t *deadline, bool *readable);
app_wifi_result_t app_wifi_channel_close(app_wifi_channel_t **channel);
