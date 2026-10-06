#include "camera_menu.h"
#include <string.h>

static void discard_relative(camera_menu_item_t *s, setting_status_t status)
{ s->remaining = 0; s->awaiting = false; s->status = status; }
static bool relative_changed_as_requested(const camera_menu_item_t *s, unsigned index)
{
    int direction;
    if (index == MENU_APERTURE) direction = s->actual > s->baseline ? 1 : -1;
    else if (!s->baseline) direction = 1; /* Bulb -> timed exposure: faster. */
    else if (!s->actual) direction = -1;
    else {
        uint64_t actual = (uint64_t)(s->actual >> 16) * (s->baseline & 0xffff);
        uint64_t baseline = (uint64_t)(s->baseline >> 16) * (s->actual & 0xffff);
        if (actual == baseline) return false; /* Equivalent fraction, no change. */
        direction = actual < baseline ? 1 : -1;
    }
    return direction == s->sent_direction;
}
void camera_menu_response(camera_menu_t *menu, unsigned index, bool accepted)
{
    if (!menu || index >= CAMERA_MENU_COUNT) return;
    camera_menu_item_t *s = &menu->items[index];
    if (s->relative) { if (!accepted) discard_relative(s, SETTING_REJECTED); }
    else setting_control_response(&s->control, accepted);
}
void camera_menu_cancel(camera_menu_t *menu)
{
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        camera_menu_item_t *s = &menu->items[i];
        if (s->remaining || s->awaiting) discard_relative(s, SETTING_REJECTED);
        if (s->control.dirty || s->control.awaiting) setting_control_response(&s->control, false);
    }
}
bool camera_menu_snapshot_apply(camera_menu_t *menu, const camera_menu_snapshot_t *snapshot, uint32_t now)
{
    if (!menu || !snapshot) return false;
    camera_choice_state_t empty = {0};
    bool valid = snapshot->valid;
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i)
        if (snapshot->states[i].count > 64) valid = false;
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        camera_menu_item_t *s = &menu->items[i];
        const camera_choice_state_t *state = valid ? &snapshot->states[i] : &empty;
        if (s->type && s->type != snapshot->types[i] && (s->control.dirty || s->control.awaiting))
            setting_control_response(&s->control, false);
        bool relative = snapshot->relative[i];
        if (s->relative != relative) {
            bool pending = s->remaining || s->awaiting || s->control.dirty || s->control.awaiting;
            discard_relative(s, pending ? SETTING_REJECTED : SETTING_IDLE);
            if (pending) setting_control_response(&s->control, false);
        }
        s->relative = relative; s->actual = state->current;
        s->code = snapshot->codes[i];
        s->type = valid ? snapshot->types[i] : 0; s->writable = state->writable && (relative || state->count >= 2);
        setting_control_snapshot(&s->control, state, now);
        if (relative) {
            if (!s->writable && (s->awaiting || s->remaining)) discard_relative(s, SETTING_REJECTED);
            else if (s->awaiting && s->actual != s->baseline) {
                if (!relative_changed_as_requested(s, i)) discard_relative(s, SETTING_REJECTED);
                else {
                    s->awaiting = false;
                    s->status = s->remaining ? SETTING_PENDING : SETTING_APPLIED;
                }
            } else if (s->awaiting && (int32_t)(now - s->deadline) >= 0) discard_relative(s, SETTING_TIMEOUT);
        }
    }
    return valid;
}
bool camera_menu_step(camera_menu_t *menu, unsigned index, int steps, bool wrap)
{
    if (!menu || index >= CAMERA_MENU_COUNT || !steps) return false;
    camera_menu_item_t *s = &menu->items[index];
    if (!s->relative) return setting_control_adjust(&s->control, steps, wrap);
    if (!s->writable) { s->status = SETTING_REJECTED; return false; }
    /* Merge opposite inputs too. Bound a long hold instead of queuing packets. */
    int64_t remaining = (int64_t)s->remaining + steps;
    s->remaining = remaining > 64 ? 64 : remaining < -64 ? -64 : (int)remaining;
    s->status = s->remaining || s->awaiting ? SETTING_PENDING : SETTING_IDLE;
    return true;
}
bool camera_menu_next(camera_menu_t *menu, unsigned index, uint32_t now, camera_menu_write_t *write)
{
    if (!menu || !write || index >= CAMERA_MENU_COUNT) return false;
    camera_menu_item_t *s = &menu->items[index]; uint32_t value;
    if (s->relative) {
        if (s->awaiting && (int32_t)(now - s->deadline) >= 0) discard_relative(s, SETTING_TIMEOUT);
        if (!s->writable || s->awaiting || !s->remaining) return false;
        int step = s->remaining > 0 ? 1 : -1;
        s->sent_direction = step;
        s->remaining -= step; s->baseline = s->actual; s->deadline = now + 10000;
        s->awaiting = true; s->status = SETTING_PENDING; value = (uint8_t)(int8_t)step;
    } else if (!setting_control_next(&s->control, now, &value)) return false;
    *write = (camera_menu_write_t){index, s->code, s->type, value, s->relative};
    return true;
}
bool camera_menu_pending(const camera_menu_t *menu)
{
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        const camera_menu_item_t *s = &menu->items[i];
        if (s->awaiting || s->remaining || s->control.awaiting || s->control.dirty) return true;
    }
    return false;
}
setting_status_t camera_menu_status(const camera_menu_t *menu, unsigned index)
{
    if (!menu || index >= CAMERA_MENU_COUNT) return SETTING_IDLE;
    const camera_menu_item_t *s = &menu->items[index];
    return s->relative ? s->status : s->control.status;
}
bool camera_menu_target(const camera_menu_t *menu, unsigned index, uint32_t *target)
{
    if (!menu || !target || index >= CAMERA_MENU_COUNT) return false;
    const camera_menu_item_t *s = &menu->items[index];
    if (s->relative || (!s->control.dirty && !s->control.awaiting)) return false;
    *target = s->control.desired; return true;
}
