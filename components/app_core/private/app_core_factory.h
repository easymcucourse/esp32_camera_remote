#pragma once
#include "network_config.h"
#include "esp_err.h"
#define APP_CORE_FACTORY_API_VERSION 1
typedef struct {
    bool (*acquire_camera)(void *context);
    void (*release_camera)(void *context);
    bool (*save_wifi)(void *context, const network_config_t *config);
    bool (*forget_camera)(void *context);
    bool (*reset_ui)(void *context);
} factory_reset_ops_t;
typedef enum { FACTORY_RESET_OK, FACTORY_RESET_BUSY, FACTORY_RESET_SAVE_FAILED,
    FACTORY_RESET_IDENTITY_FAILED, FACTORY_RESET_ROLLBACK_FAILED, FACTORY_RESET_UI_FAILED,
    FACTORY_RESET_INVALID } factory_reset_result_t;
/* Synchronous transaction. Success retains camera ownership until reboot.
 * Failure releases camera and attempts only Wi-Fi rollback. Identity/UI writes
 * may be partial; the result never promises rollback of their namespaces. */
factory_reset_result_t factory_reset_all(const network_config_t *current,
    const factory_reset_ops_t *ops, void *context);
