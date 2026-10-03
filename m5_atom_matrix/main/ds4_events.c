#include "ds4_events.h"
#include "atom_protocol.h"

void ds4_events_push(ds4_events_t *queue, uint32_t buttons)
{
    buttons &= ATOM_BUTTON_MASK & ~ATOM_LOCAL_MASK;
    if (buttons == queue->last_buttons) return;
    queue->last_buttons = buttons;
    if (queue->count == DS4_EVENT_CAPACITY) {
        queue->head = (queue->head + 1) % DS4_EVENT_CAPACITY;
        --queue->count;
        ++queue->dropped;
        queue->gap_pending = true;
    }
    if (++queue->next_id == 0) ++queue->next_id;
    unsigned tail = (queue->head + queue->count) % DS4_EVENT_CAPACITY;
    queue->entries[tail] = (ds4_event_t){queue->next_id, buttons, false};
    ++queue->count;
}

bool ds4_events_read(ds4_events_t *queue, uint32_t ack_id, ds4_event_t *event)
{
    if (queue->count && ack_id && queue->entries[queue->head].id == ack_id) {
        if (ack_id == queue->gap_id && queue->gap_dropped == queue->dropped)
            queue->gap_pending = false;
        queue->head = (queue->head + 1) % DS4_EVENT_CAPACITY;
        --queue->count;
    }
    if (!queue->count) return false;
    *event = queue->entries[queue->head];
    event->gap = queue->gap_pending;
    if (event->gap) { queue->gap_id = event->id; queue->gap_dropped = queue->dropped; }
    return true;
}
