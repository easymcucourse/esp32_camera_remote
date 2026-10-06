#pragma once
#include "app_wifi.h"
/* Single ESP-IDF radio; a second live factory returns STATE. Factory creation
 * allocates state/locks only, allowing saved config read before LCD startup.
 * app_wifi_init initializes radio/netif/events at the composition owner's
 * selected startup point, with cleanup on partial failure. */
app_wifi_result_t wifi_esp32_create(app_wifi_t **wifi);
