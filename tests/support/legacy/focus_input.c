#include "focus_input.h"

int focus_input_update(focus_input_t *s, uint32_t buttons, bool eligible,
                       bool event, uint32_t now)
{
    uint32_t keys = buttons & FOCUS_KEYS;
    if (!eligible || eligible != s->eligible) {
        s->armed = false;
        s->held = 0;
    }
    s->eligible = eligible;
    if (!keys) {
        s->held = 0;
        s->armed = eligible;
        return 0;
    }
    if (!eligible || !s->armed) return 0;
    if (keys == FOCUS_KEYS || (s->held && keys != s->held)) {
        s->held = 0;
        s->armed = false;
        return 0;
    }
    if (!s->held) {
        if (!event) return 0;
        s->held = keys;
        s->next_ms = now + 400;
        return keys == FOCUS_KEY_X ? 1 : -1;
    }
    if (!event && (int32_t)(now - s->next_ms) >= 0) {
        s->next_ms = now + 150; /* No catch-up burst after a delay. */
        return keys == FOCUS_KEY_X ? 1 : -1;
    }
    return 0;
}
