#include "ds4_events.h"

void ds4_events_push(ds4_events_t *queue, uint32_t buttons)
{
    if (queue->count == DS4_EVENT_CAPACITY) {
        queue->head = (queue->head + 1) % DS4_EVENT_CAPACITY;
        --queue->count;
        ++queue->dropped;
    }
    if (++queue->next_id == 0) ++queue->next_id;
    unsigned tail = (queue->head + queue->count) % DS4_EVENT_CAPACITY;
    queue->entries[tail] = (ds4_event_t){queue->next_id, buttons};
    ++queue->count;
}

bool ds4_events_read(ds4_events_t *queue, uint32_t ack_id, ds4_event_t *event)
{
    if (queue->count && ack_id && queue->entries[queue->head].id == ack_id) {
        queue->head = (queue->head + 1) % DS4_EVENT_CAPACITY;
        --queue->count;
    }
    if (!queue->count) return false;
    *event = queue->entries[queue->head];
    return true;
}
