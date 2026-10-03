#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_http_server.h"
esp_err_t maint_ota_check(httpd_req_t *request);
esp_err_t maint_ota_upload(httpd_req_t *request);
esp_err_t maint_ota_status(httpd_req_t *request);
bool maint_ota_display(char *text,size_t size);
void maint_ota_startup_ready(void);
void maint_ota_health(void);
const char *maint_ota_boot_status(void);
