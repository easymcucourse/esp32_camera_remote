#pragma once
#include <stdbool.h>
#include <stdint.h>

#define FOCUS_KEY_X (1u << 15) /* DS4 Square */
#define FOCUS_KEY_Y (1u << 12) /* DS4 Triangle */
#define FOCUS_KEYS (FOCUS_KEY_X | FOCUS_KEY_Y)
typedef struct {
    bool armed, eligible;
    uint32_t held, next_ms;
} focus_input_t;
/* Events generate the first step; snapshots only repeat/observe release.
 * +1 = near, -1 = far, 0 = no command. Call update(false) on loss. */
int focus_input_update(focus_input_t *state, uint32_t buttons, bool eligible,
                       bool event, uint32_t now_ms);
