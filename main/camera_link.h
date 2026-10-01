#pragma once
#include <stdbool.h>
#include <stdint.h>
/* No network or RTOS dependencies: discovery and retry policy. */
bool camera_candidate_allowed(bool paired, const uint8_t saved_mac[6], const uint8_t candidate_mac[6]);
/* -1 none; -2 ambiguous; otherwise bit index of the sole reachable candidate. */
int camera_select_candidate(uint32_t reachable);
unsigned camera_retry_delay(unsigned failures);
