#pragma once
#include <stdbool.h>
#include <stdint.h>
void app_core_messages_poll(void);
/* Core sole System consumer, after every functional worker/endpoint has
 * stopped. Retire System then join the router; timeout preserves leases/tasks
 * and closed admission. No business callbacks are executed during this stop. */
bool app_core_messages_quiesce(uint32_t timeout_ms);
