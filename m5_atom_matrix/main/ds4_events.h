#pragma once
#include <stdbool.h>
#include <stdint.h>

#define DS4_EVENT_CAPACITY 128
typedef struct { uint32_t id, buttons; bool gap; } ds4_event_t;
typedef struct {
    ds4_event_t entries[DS4_EVENT_CAPACITY];
    unsigned head, count;
    uint32_t next_id, dropped;
    uint32_t last_buttons, gap_id, gap_dropped;
    bool gap_pending;
} ds4_events_t;

// Caller supplies synchronization. Zero initialization creates an empty queue.
void ds4_events_push(ds4_events_t *queue, uint32_t buttons);
bool ds4_events_read(ds4_events_t *queue, uint32_t ack_id, ds4_event_t *event);
