#include <assert.h>
#include <stdio.h>
#include "ds4_events.h"

int main(void)
{
    ds4_events_t queue = {0};
    ds4_event_t event;
    assert(!ds4_events_read(&queue, 0, &event));
    ds4_events_push(&queue, 2); /* L3 is local and must not consume capacity. */
    assert(queue.count == 0);
    // Two complete short Start presses between LCD polls retain all four edges.
    ds4_events_push(&queue, 8);
    ds4_events_push(&queue, 0);
    ds4_events_push(&queue, 8);
    ds4_events_push(&queue, 0);
    assert(ds4_events_read(&queue, 0, &event) && event.buttons == 8);
    uint32_t first = event.id;
    assert(ds4_events_read(&queue, 0, &event) && event.id == first);
    assert(ds4_events_read(&queue, 999, &event) && event.id == first);
    assert(ds4_events_read(&queue, first, &event) && event.buttons == 0);
    uint32_t second = event.id;
    assert(ds4_events_read(&queue, first, &event) && event.id == second);
    assert(ds4_events_read(&queue, second, &event) && event.buttons == 8);
    uint32_t third = event.id;
    assert(ds4_events_read(&queue, third, &event) && event.buttons == 0);
    assert(!ds4_events_read(&queue, event.id, &event));
    queue.next_id = UINT32_MAX;
    ds4_events_push(&queue, 1);
    assert(ds4_events_read(&queue, 0, &event) && event.id == 1);
    assert(!ds4_events_read(&queue, event.id, &event));
    for (unsigned i = 0; i < DS4_EVENT_CAPACITY + 3; ++i) ds4_events_push(&queue, i << 2);
    assert(queue.count == DS4_EVENT_CAPACITY && queue.dropped == 3);
    uint32_t ack = 0;
    for (unsigned i = 3; i < DS4_EVENT_CAPACITY + 3; ++i) {
        assert(ds4_events_read(&queue, ack, &event) && event.buttons == (i << 2));
        assert(event.gap == (i == 3));
        if (i == 3) {
            ds4_event_t repeat;
            assert(ds4_events_read(&queue, 0, &repeat) && repeat.id == event.id && repeat.gap);
        }
        ack = event.id;
    }
    assert(!ds4_events_read(&queue, ack, &event));
    assert(!queue.gap_pending);
    queue = (ds4_events_t){0};
    ds4_events_push(&queue, 8); ds4_events_push(&queue, 10);
    assert(queue.count == 1 && queue.last_buttons == 8);
    for (unsigned i = 0; i < DS4_EVENT_CAPACITY + 1; ++i) ds4_events_push(&queue, (i + 1) << 4);
    assert(ds4_events_read(&queue, 0, &event) && event.gap);
    uint32_t old_gap = event.id;
    ds4_events_push(&queue, 1); /* Overflow again before acknowledging the gap. */
    assert(ds4_events_read(&queue, old_gap, &event) && event.gap && event.id != old_gap);
    assert(queue.gap_pending);
    puts("DS4 event cache tests passed");
}
