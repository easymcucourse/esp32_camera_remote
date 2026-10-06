#pragma once
#include "esp_err.h"
#define APP_CORE_API_VERSION 1
/* Called once by ESP-IDF app_main on its original startup stack. Core owns
 * top-level composition and freezes subscriptions after fixed endpoints start.
 * A failed attempt closes HTTP and drains normal owners; failed drains stay
 * closed until app_main aborts. No partial-init retry, no NVS erase. */
esp_err_t app_core_start(void);
