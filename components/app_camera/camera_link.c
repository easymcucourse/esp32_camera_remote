#include "camera_link.h"
#include <string.h>
bool camera_candidate_allowed(bool paired, const uint8_t saved_mac[6], const uint8_t candidate_mac[6])
{
    return !paired || !memcmp(saved_mac, candidate_mac, 6);
}
int camera_select_candidate(uint32_t reachable)
{
    if (!reachable) return -1;
    if (reachable & (reachable - 1)) return -2;
    int i=0;
    while (!(reachable & 1)) { reachable >>= 1; ++i; }
    return i;
}
unsigned camera_retry_delay(unsigned failures)
{
    if (failures >= 5) return 30;
    return 1u << failures;
}
