#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Copied scalar choices for target merging/readback. No protocol or backend
 * object is retained; signed values keep their width-limited bit pattern. */
typedef struct {
    uint32_t values[64], current;
    unsigned count;
    bool writable;
} camera_choice_state_t;
