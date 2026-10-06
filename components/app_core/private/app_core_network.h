#pragma once
#include "esp_err.h"
#include <stdint.h>
/* Normal bridge/channel workers, only after router and startup trigger. */
esp_err_t app_core_network_messages_start(void);
/* Core owner only, after UART/Input/Camera/bench/menu stopped. Shared
 * budget joins config first, then bridge/channel workers. Timeout retains the
 * Wi-Fi object and unfinished owners; retry, never activate maintenance early. */
esp_err_t app_core_network_quiesce(uint32_t timeout_ms);
