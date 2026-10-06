#pragma once
#include <stdint.h>
/* Shared Camera domain lifetime sequence; unrelated to PTP transactions,
 * endpoint epochs, network generation or input safety generation. */
uint32_t camera_session_next_generation(void);
