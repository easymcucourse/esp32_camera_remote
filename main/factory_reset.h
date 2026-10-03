#pragma once
#include "wifi_config.h"
typedef struct {
    bool (*acquire_camera)(void *context);
    void (*release_camera)(void *context);
    bool (*save_wifi)(void *context, const app_wifi_config_t *config);
    bool (*forget_camera)(void *context);
    bool (*reset_ui)(void *context);
} factory_reset_ops_t;
typedef enum { FACTORY_RESET_OK, FACTORY_RESET_BUSY, FACTORY_RESET_SAVE_FAILED,
               FACTORY_RESET_IDENTITY_FAILED, FACTORY_RESET_ROLLBACK_FAILED, FACTORY_RESET_UI_FAILED } factory_reset_result_t;
/* Success retains the camera lease until the caller reboots. Failure releases it.
 * Identity erase or UI persistence failure can be partial: only Wi-Fi rollback
 * is attempted; never claim rollback of the camera/UI namespaces. */
factory_reset_result_t factory_reset_all(const app_wifi_config_t *current,
                                        const factory_reset_ops_t *ops, void *context);
