#include <assert.h>
#include <stdio.h>
#include "focus_input.h"

int main(void)
{
    focus_input_t s = {0};
    assert(focus_input_update(&s, FOCUS_KEY_X, true, true, 0) == 0); // held on connect
    assert(focus_input_update(&s, 0, true, false, 10) == 0);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 20) == 0); // snapshot cannot press
    assert(focus_input_update(&s, FOCUS_KEY_X, true, true, 25) == 1);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 424) == 0);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 425) == 1);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, true, 500) == 0); // duplicate event
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 2000) == 1); // no catchup
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 2001) == 0);
    assert(focus_input_update(&s, FOCUS_KEYS, true, true, 2100) == 0);
    assert(focus_input_update(&s, FOCUS_KEY_Y, true, true, 2200) == 0); // locked
    focus_input_update(&s, 0, true, false, 2300);
    assert(focus_input_update(&s, FOCUS_KEY_Y, true, true, 2400) == -1);
    assert(focus_input_update(&s, FOCUS_KEY_Y, false, false, 2500) == 0);
    assert(focus_input_update(&s, FOCUS_KEY_Y, true, true, 2600) == 0); // mode/session changed
    focus_input_update(&s, 0, true, false, 2700);
    assert(focus_input_update(&s, FOCUS_KEY_Y, true, true, 2800) == -1);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, true, 2801) == 0); // direction switch
    s = (focus_input_t){0};
    focus_input_update(&s, 0, true, false, 0xfffffff0u);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, true, 0xfffffff0u) == 1);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 383) == 0);
    assert(focus_input_update(&s, FOCUS_KEY_X, true, false, 384) == 1);
    puts("focus input tests passed");
    return 0;
}
