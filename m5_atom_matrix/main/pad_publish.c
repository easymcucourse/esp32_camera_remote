#include "pad_publish.h"
#include "atom_protocol.h"
unsigned pad_publish_source(bool classic, bool ble)
{ return classic ? PAD_SOURCE_DS4 : ble ? PAD_SOURCE_BLE : PAD_SOURCE_NONE; }
void pad_publish_reset(pad_publish_t *p)
{
    p->events.head=p->events.count=0;p->events.last_buttons=0;p->events.gap_pending=false;
    ++p->generation;p->source=PAD_SOURCE_NONE;p->state=(ds4_state_t){.battery=255};
}
void pad_publish_apply(pad_publish_t *p, unsigned source, const ds4_state_t *next)
{
    ds4_state_t clean=*next;
    if (!clean.connected) { clean=(ds4_state_t){.battery=255};source=PAD_SOURCE_NONE; }
    clean.buttons &= ATOM_BUTTON_MASK;
    if (source!=p->source) {
        /* Source changes publish only a baseline, never a synthetic press.
         * Keep next_id and dropped monotonically advancing across resets. */
        ++p->generation;p->source=source;
        p->events.head=p->events.count=0;p->events.last_buttons=clean.buttons;p->events.gap_pending=false;
    }
    if (p->state.connected!=clean.connected) ++p->generation;
    if (p->state.buttons!=clean.buttons) ds4_events_push(&p->events,clean.buttons);
    p->state=clean;
}

unsigned pad_publish_mode_source(unsigned mode, bool classic, bool ble)
{ return mode==ATOM_INPUT_DS ? (classic?PAD_SOURCE_DS4:PAD_SOURCE_NONE) : mode==ATOM_INPUT_XBOX && ble ? PAD_SOURCE_BLE : PAD_SOURCE_NONE; }
