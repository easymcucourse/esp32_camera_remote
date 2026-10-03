#include "camera_actions.h"

static void release_all(camera_actions_t *q)
{
    q->head = q->count = 0;
    if (++q->generation == 0) ++q->generation;
    q->release_pending = true;
}
void camera_actions_session(camera_actions_t *q, bool open)
{
    release_all(q);
    q->session = open;
    q->s1 = q->s2 = false;
    q->zoom = 0;
    q->release_pending = false;
}
bool camera_actions_submit(camera_actions_t *q, pad_action_t action, uint32_t now)
{
    if (!q->session || action.generation != q->generation) return false;
    if (action.type == PAD_ACTION_RELEASE_ALL) { release_all(q); return true; }
    if (action.type != PAD_ACTION_S1 && action.type != PAD_ACTION_S2 &&
        action.type != PAD_ACTION_ZOOM && action.type != PAD_ACTION_RECORD) return false;
    if ((action.type == PAD_ACTION_ZOOM && (action.value < -1 || action.value > 1)) ||
        (action.type != PAD_ACTION_ZOOM && action.value != 0 && action.value != 1)) return false;
    if (q->count == CAMERA_ACTION_CAPACITY) { release_all(q); return false; }
    size_t tail = (q->head + q->count) % CAMERA_ACTION_CAPACITY;
    bool release = action.type != PAD_ACTION_RECORD && action.value == 0;
    q->entries[tail] = (camera_action_t){action, now, release};
    ++q->count;
    return true;
}
bool camera_actions_current(const camera_actions_t *q, const camera_action_t *a)
{
    return q->session && q->generation == a->action.generation;
}
bool camera_actions_record_queued(const camera_actions_t *q)
{
    for (size_t i = 0; i < q->count; ++i)
        if (q->entries[(q->head + i) % CAMERA_ACTION_CAPACITY].action.type == PAD_ACTION_RECORD) return true;
    return false;
}
void camera_actions_complete(camera_actions_t *q, const camera_action_t *a, bool accepted)
{
    if (!q->session || !accepted || !a->release) return;
    if (a->action.type == PAD_ACTION_S2) q->s2 = false;
    if (a->action.type == PAD_ACTION_S1) q->s1 = false;
    if (a->action.type == PAD_ACTION_ZOOM) q->zoom = 0;
}
bool camera_actions_next(camera_actions_t *q, uint32_t now, camera_action_t *out)
{
    if (!q->session || !out) return false;
    for (;;) {
        if (q->release_pending) {
            *out = (camera_action_t){{PAD_ACTION_RELEASE_ALL, 0, q->generation}, now, true};
            if (q->s2) { out->action.type = PAD_ACTION_S2; return true; }
            if (q->s1) { out->action.type = PAD_ACTION_S1; return true; }
            if (q->zoom) { out->action.type = PAD_ACTION_ZOOM; return true; }
            q->release_pending = false;
        }
        if (!q->count) return false;
        *out = q->entries[q->head];
        q->head = (q->head + 1) % CAMERA_ACTION_CAPACITY;
        --q->count;
        if (!camera_actions_current(q, out)) continue;
        if (!out->release && (uint32_t)(now - out->queued_ms) >= 1000) {
            release_all(q); continue;
        }
        if (out->action.type == PAD_ACTION_S1 && out->action.value) q->s1 = true;
        if (out->action.type == PAD_ACTION_S2 && out->action.value) q->s2 = true;
        if (out->action.type == PAD_ACTION_ZOOM && out->action.value) q->zoom = out->action.value;
        return true;
    }
}
