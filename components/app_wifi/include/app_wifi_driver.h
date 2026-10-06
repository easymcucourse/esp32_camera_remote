#pragma once
#include "app_wifi.h"

/* Implementation factory contract, consumed only by a backend / composition
 * root. Ops must outlive the object. Context ownership transfers on successful
 * bind only; failed bind leaves it with the factory. No mutable global backend. */
typedef struct {
    unsigned api_version;
    uint32_t capabilities;
    app_wifi_result_t (*start)(void *context, const network_config_t *config);
    app_wifi_result_t (*reconfigure)(void *context, const network_config_t *config);
    app_wifi_result_t (*stop)(void *context, uint32_t timeout_ms);
    app_wifi_result_t (*status)(void *context, app_wifi_status_t *status);
    app_wifi_result_t (*clients)(void *context, app_wifi_client_t *out, size_t capacity, size_t *count);
    void (*destroy)(void *context);
    app_wifi_result_t (*init)(void *context);
    app_wifi_result_t (*saved_read)(void *context, network_config_t *config);
    app_wifi_result_t (*saved_write)(void *context, const network_config_t *config);
    app_wifi_result_t (*config_get)(void *, network_config_t *);
    app_wifi_result_t (*config_apply)(void *, const network_config_t *, bool, uint32_t *);
    app_wifi_result_t (*config_commit)(void *, uint32_t, unsigned);
    app_wifi_result_t (*config_cancel)(void *, uint32_t);
    app_wifi_result_t (*config_result)(void *, uint32_t, app_wifi_result_t *);
    app_wifi_result_t (*config_freeze)(void *, uint32_t);
    void (*config_resume)(void *);
    app_wifi_result_t (*channel_open)(void *, const app_wifi_endpoint_t *, const app_wifi_deadline_t *, void **);
    app_wifi_result_t (*channel_io)(void *, void *, size_t, bool, const app_wifi_deadline_t *, size_t *);
    void (*channel_close)(void *);
    app_wifi_result_t (*channel_poll)(void *, const app_wifi_deadline_t *, bool *);
    app_wifi_result_t (*config_quiesce)(void *, uint32_t); /* Optional. AP stays online. */
    app_wifi_result_t (*config_start)(void *); /* Optional. Same worker/new owner. */
} app_wifi_driver_ops_t;
app_wifi_result_t app_wifi_driver_bind(const app_wifi_driver_ops_t *ops,
    void *context, app_wifi_t **wifi);
