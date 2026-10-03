#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { OTA_HEALTH_WAIT, OTA_HEALTH_CONFIRM, OTA_HEALTH_ROLLBACK } ota_health_action_t;

/* Called only for a pending image, after all startup tasks are ready. Display
 * failure is handled by the application's existing drain-and-restart path. */
static inline ota_health_action_t ota_health_decide(int64_t elapsed_us,
                                                    bool heap_ok, bool ap_up)
{
    if (!heap_ok) return OTA_HEALTH_ROLLBACK;
    if (elapsed_us < INT64_C(60000000)) return OTA_HEALTH_WAIT;
    return ap_up ? OTA_HEALTH_CONFIRM : OTA_HEALTH_ROLLBACK;
}
