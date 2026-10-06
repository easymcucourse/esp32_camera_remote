#pragma once
#include "esp_http_server.h"
#include <stdbool.h>
/* True only when the full route may execute. Otherwise response is complete.
 * Unknown path/method handlers use the same gate as all registered routes. */
bool maintenance_trigger_route(httpd_req_t *request);
