#include <assert.h>
#include <stdio.h>
#include "setting_control.h"
#include "sony_props.h" /* Original fixture value name, aliases generic choices. */
int main(void)
{
    sony_mode_state_t m = {.values = {1, 2, 3, 4}, .current = 1, .count = 4, .writable = true};
    setting_control_t s = {0}; uint32_t value;
    setting_control_snapshot(&s, &m, 0);
    assert(setting_control_step(&s, 1));
    assert(setting_control_next(&s, 0, &value) && value == 2);
    setting_control_response(&s, true);
    /* Accepted request + stale readback must not be reported applied. */
    setting_control_snapshot(&s, &m, 5000);
    assert(s.awaiting && s.status == SETTING_PENDING);
    /* Nine further presses merge around the latest target, not stale actual. */
    for (int i = 0; i < 9; ++i) assert(setting_control_step(&s, 1));
    assert(s.desired == 3 && !setting_control_next(&s, 6000, &value));
    m.current = 2; setting_control_snapshot(&s, &m, 6500);
    assert(setting_control_next(&s, 6500, &value) && value == 3);
    setting_control_response(&s, true);
    m.current = 3; setting_control_snapshot(&s, &m, 7000);
    assert(s.status == SETTING_APPLIED && !s.awaiting && !s.dirty);
    /* Negative directions, response rejection, timeout, enum removal. */
    assert(setting_control_step(&s, -1));
    assert(setting_control_next(&s, 8000, &value) && value == 2);
    setting_control_response(&s, false);
    assert(s.status == SETTING_REJECTED && s.desired == 3 && !s.dirty);
    assert(setting_control_step(&s, 1));
    assert(setting_control_next(&s, 9000, &value));
    setting_control_snapshot(&s, &m, 19000);
    assert(s.status == SETTING_TIMEOUT && !s.awaiting);
    assert(setting_control_step(&s, 1));
    m.count = 3; setting_control_snapshot(&s, &m, 20000);
    assert(s.status == SETTING_REJECTED && !s.dirty);
    m.writable = false;
    setting_control_snapshot(&s, &m, 20001);
    assert(!setting_control_step(&s, 1));
    m.writable = true; m.count = 4; m.current = 99;
    setting_control_snapshot(&s, &m, 21000);
    assert(!setting_control_step(&s, 1));
    /* Deadline across uint32 wrap, and readback at the deadline wins. */
    m.current = 1; setting_control_snapshot(&s, &m, UINT32_MAX - 10);
    assert(setting_control_step(&s, 1));
    assert(setting_control_next(&s, UINT32_MAX - 10, &value));
    setting_control_snapshot(&s, &m, 0); assert(s.awaiting);
    m.current = 2; setting_control_snapshot(&s, &m, 9989);
    assert(s.status == SETTING_APPLIED);
    puts("setting target merging tests passed");
    return 0;
}
