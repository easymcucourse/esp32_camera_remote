#include <assert.h>
#include <stdio.h>
#include "camera_actions.h"
static void put(camera_actions_t *q, pad_action_type_t type, int value, uint32_t now)
{
    assert(camera_actions_submit(q, (pad_action_t){type, value, q->generation}, now));
}
static void take(camera_actions_t *q, pad_action_type_t type, int value, uint32_t now)
{
    camera_action_t a;
    assert(camera_actions_next(q, now, &a));
    assert(a.action.type == type && a.action.value == value);
    camera_actions_complete(q, &a, true);
}
int main(void)
{
    camera_actions_t q = {0}; camera_action_t a;
    camera_actions_session(&q, true);
    put(&q, PAD_ACTION_S1, 1, 0); put(&q, PAD_ACTION_S2, 1, 0);
    put(&q, PAD_ACTION_S2, 0, 1); put(&q, PAD_ACTION_S1, 0, 1);
    take(&q, PAD_ACTION_S1, 1, 2); take(&q, PAD_ACTION_S2, 1, 2);
    take(&q, PAD_ACTION_S2, 0, 2); take(&q, PAD_ACTION_S1, 0, 2);
    assert(!camera_actions_next(&q, 2, &a));
    /* Controls are pessimistically latched before socket I/O. */
    put(&q, PAD_ACTION_S1, 1, 3); take(&q, PAD_ACTION_S1, 1, 3);
    put(&q, PAD_ACTION_S2, 1, 3); take(&q, PAD_ACTION_S2, 1, 3);
    put(&q, PAD_ACTION_ZOOM, -1, 3); take(&q, PAD_ACTION_ZOOM, -1, 3);
    uint32_t old = q.generation;
    put(&q, PAD_ACTION_RECORD, 1, 3);
    assert(camera_actions_record_queued(&q));
    put(&q, PAD_ACTION_RELEASE_ALL, 0, 4);
    assert(!camera_actions_record_queued(&q));
    assert(q.generation != old);
    take(&q, PAD_ACTION_S2, 0, 4); take(&q, PAD_ACTION_S1, 0, 4);
    take(&q, PAD_ACTION_ZOOM, 0, 4); assert(!camera_actions_next(&q, 4, &a));
    assert(!camera_actions_submit(&q, (pad_action_t){PAD_ACTION_S1, 1, old}, 5));
    /* Saturating normal capacity still schedules all required releases. */
    put(&q, PAD_ACTION_S1, 1, 5); take(&q, PAD_ACTION_S1, 1, 5);
    put(&q, PAD_ACTION_S2, 1, 5); take(&q, PAD_ACTION_S2, 1, 5);
    for (unsigned i = 0; i < CAMERA_ACTION_CAPACITY; ++i) put(&q, PAD_ACTION_ZOOM, 1, 5);
    assert(!camera_actions_submit(&q, (pad_action_t){PAD_ACTION_S2, 0, q.generation}, 5));
    take(&q, PAD_ACTION_S2, 0, 5); take(&q, PAD_ACTION_S1, 0, 5);
    assert(!camera_actions_next(&q, 5, &a));
    /* Old presses are cancelled; releases never expire. */
    put(&q, PAD_ACTION_S1, 1, 10);
    assert(!camera_actions_next(&q, 1010, &a));
    put(&q, PAD_ACTION_S1, 1, 1020); take(&q, PAD_ACTION_S1, 1, 1020);
    put(&q, PAD_ACTION_S1, 0, 1020); take(&q, PAD_ACTION_S1, 0, 10000);
    put(&q, PAD_ACTION_S1, 1, UINT32_MAX - 10);
    take(&q, PAD_ACTION_S1, 1, 0);
    /* A cancellation racing a popped press invalidates it, then releases. */
    put(&q, PAD_ACTION_S2, 1, 20); assert(camera_actions_next(&q, 20, &a));
    assert(camera_actions_current(&q, &a)); put(&q, PAD_ACTION_RELEASE_ALL, 0, 21);
    assert(!camera_actions_current(&q, &a));
    take(&q, PAD_ACTION_S2, 0, 21); take(&q, PAD_ACTION_S1, 0, 21);
    /* Release popped, then invalidated: latch stays set until the retry ACK. */
    put(&q, PAD_ACTION_S1, 1, 22); take(&q, PAD_ACTION_S1, 1, 22);
    put(&q, PAD_ACTION_S1, 0, 23); assert(camera_actions_next(&q, 23, &a));
    assert(q.s1); camera_actions_complete(&q, &a, false); assert(q.s1);
    put(&q, PAD_ACTION_RELEASE_ALL, 0, 24); assert(!camera_actions_current(&q, &a));
    take(&q, PAD_ACTION_S1, 0, 24); assert(!q.s1);
    camera_actions_session(&q, false);
    assert(!camera_actions_next(&q, 22, &a));
    assert(!camera_actions_submit(&q, (pad_action_t){PAD_ACTION_S1, 1, q.generation}, 22));
    camera_actions_session(&q, true); assert(!camera_actions_next(&q, 23, &a));
    assert(!camera_actions_submit(&q, (pad_action_t){PAD_ACTION_S1, 2, q.generation}, 23));
    puts("camera action ordering and release tests passed");
    return 0;
}
