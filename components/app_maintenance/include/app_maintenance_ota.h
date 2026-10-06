#pragma once
#include <stdbool.h>
#include "esp_http_server.h"
#define APP_MAINTENANCE_OTA_API_VERSION 1
/* Core injects system admission/reboot operations before publishing HTTP.
 * No functional application, router, camera reservation or UI is known here.
 * HTTP body and JSON result buffers remain owned by this synchronous handler.
 * Upload is single-owner, aborted on shutdown, and releases admission on every
 * admitted terminal path. After validated image/boot metadata commit, reboot
 * is committed even if the response is lost. Callbacks must remain valid until reboot.
 */
typedef struct {
    bool (*restart_prepare)(void *context);
    void (*restart_cancel)(void *context);
    bool (*restart_commit)(void *context,unsigned delay_ms);
    esp_err_t (*upload_begin)(void *context);
    void (*upload_end)(void *context);
    bool (*shutting_down)(void *context);
    void *context;
} app_maintenance_ota_ops_t;
esp_err_t app_maintenance_ota_init(const app_maintenance_ota_ops_t *ops);
esp_err_t app_maintenance_ota_check(httpd_req_t *request);
esp_err_t app_maintenance_ota_upload(httpd_req_t *request);
esp_err_t app_maintenance_ota_status(httpd_req_t *request);
const char *app_maintenance_ota_boot_status(void);
