#pragma once
#include "app_wifi.h"
#include "esp_err.h"
#include "preferences_config.h"
#define APP_MAINTENANCE_WEB_API_VERSION 1
#define APP_MAINTENANCE_SETTINGS_VERSION PREFERENCES_CONFIG_VERSION
/* Persistent values, never normal application's runtime state. Loaded on reboot. */
typedef preferences_config_t app_maintenance_settings_t;
typedef struct {
    bool (*available)(void *context);
    void (*touch)(void *context);
    bool (*restart_prepare)(void *context);
    void (*restart_cancel)(void *context);
    bool (*restart_commit)(void *context,unsigned delay_ms);
    esp_err_t (*settings_read)(void *context,app_maintenance_settings_t *settings);
    esp_err_t (*settings_write)(void *context,const app_maintenance_settings_t *settings);
    /* Exclusive persistent reset, no normal application runtime calls. The
     * caller reserves reboot first; success stays frozen until reboot. Failure
     * may leave identity/preferences partially reset; only Wi-Fi rolls back. */
    esp_err_t (*factory_reset)(void *context,bool all);
    /* After config COMMIT succeeds: Core observes the bounded token result,
     * commits reserved reboot only on success, cancels reservation on failure. */
    void (*wifi_committed)(void *context,uint32_t token);
    void *context;
} app_maintenance_web_ops_t;
/* Core calls init before publishing HTTP. Only the owner starts/stops HTTP;
 * stop is never called from its own server task. Failed stop retains ownership.
 * All handlers run on the one original HTTP task; no functional message bus.
 * Stop closes Web admission and asks SDK to close sessions before joining.
 * Session close is queued; a blocked handler waits for the existing 10s I/O
 * timeout. JSON receives recheck admission; Core's OTA shutdown callback must
 * report closure during active uploads. SDK join has no strict project deadline.
 */
esp_err_t app_maintenance_web_init(app_wifi_t *wifi,const app_maintenance_web_ops_t *ops);
esp_err_t app_maintenance_web_start(void);
esp_err_t app_maintenance_web_stop(void);
