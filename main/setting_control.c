#include "setting_control.h"

static unsigned index_of(const sony_mode_state_t *m, uint32_t value)
{
    unsigned i = 0;
    while (i < m->count && m->values[i] != value) ++i;
    return i;
}
static void discard(setting_control_t *s, setting_status_t status)
{
    s->dirty = s->awaiting = false;
    s->desired = s->snapshot.current;
    s->status = status;
}
void setting_control_snapshot(setting_control_t *s, const sony_mode_state_t *snapshot, uint32_t now)
{
    s->snapshot = *snapshot;
    if (s->awaiting && snapshot->current == s->sent && snapshot->count) {
        s->awaiting = false;
        s->status = s->desired == snapshot->current ? SETTING_APPLIED : SETTING_PENDING;
    }
    if (s->dirty || s->awaiting) {
        if (!s->awaiting && s->desired == snapshot->current && snapshot->count) {
            discard(s, SETTING_APPLIED);
        } else if (!snapshot->writable || !snapshot->count || snapshot->count > 64 ||
            index_of(snapshot, s->desired) == snapshot->count ||
            (s->awaiting && index_of(snapshot, s->sent) == snapshot->count)) {
            discard(s, SETTING_REJECTED);
        } else if (s->awaiting && (int32_t)(now - s->deadline_ms) >= 0) {
            discard(s, SETTING_TIMEOUT);
        }
    } else s->desired = snapshot->current;
}
bool setting_control_adjust(setting_control_t *s, int steps, bool wrap)
{
    if (!steps) return false;
    sony_mode_state_t *m = &s->snapshot;
    uint32_t base = s->dirty || s->awaiting ? s->desired : m->current;
    if (!m->writable || m->count < 2 || m->count > 64) {
        s->status = SETTING_REJECTED;
        return false;
    }
    unsigned index = index_of(m, base);
    if (index == m->count) { s->status = SETTING_REJECTED; return false; }
    int next;
    if (wrap) next = ((int)index + steps % (int)m->count + (int)m->count) % (int)m->count;
    else if (steps > (int)m->count - 1 - (int)index) next = (int)m->count - 1;
    else if (steps < -(int)index) next = 0;
    else next = (int)index + steps;
    s->desired = m->values[next];
    s->dirty = true;
    s->status = SETTING_PENDING;
    if (!s->awaiting && s->desired == m->current) discard(s, SETTING_APPLIED);
    return true;
}
bool setting_control_step(setting_control_t *s, int steps)
{ return setting_control_adjust(s, steps, true); }
bool setting_control_next(setting_control_t *s, uint32_t now, uint32_t *value)
{
    if (s->awaiting && (int32_t)(now - s->deadline_ms) >= 0) discard(s, SETTING_TIMEOUT);
    if (!value || s->awaiting || !s->dirty) return false;
    s->sent = s->desired;
    s->awaiting = true;
    s->deadline_ms = now + 10000;
    s->status = SETTING_PENDING;
    *value = s->sent;
    return true;
}
void setting_control_response(setting_control_t *s, bool accepted)
{
    if (!accepted) discard(s, SETTING_REJECTED);
}
