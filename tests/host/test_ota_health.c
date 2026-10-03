#include "ota_health.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    /* AP startup may be incomplete during the observation window, but cannot
     * leave an unconfirmed image running indefinitely once the deadline passes. */
    assert(ota_health_decide(0, true, false) == OTA_HEALTH_WAIT);
    assert(ota_health_decide(59999999, true, false) == OTA_HEALTH_WAIT);
    assert(ota_health_decide(59999999, true, true) == OTA_HEALTH_WAIT);
    assert(ota_health_decide(60000000, true, true) == OTA_HEALTH_CONFIRM);
    assert(ota_health_decide(60000000, true, false) == OTA_HEALTH_ROLLBACK);
    assert(ota_health_decide(INT64_MAX, true, false) == OTA_HEALTH_ROLLBACK);
    for (int ap=0; ap<2; ++ap) {
        assert(ota_health_decide(0, false, ap) == OTA_HEALTH_ROLLBACK);
        assert(ota_health_decide(60000000, false, ap) == OTA_HEALTH_ROLLBACK);
    }
    puts("OTA self-test boundaries passed");
    return 0;
}
