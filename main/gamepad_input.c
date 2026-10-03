#include "gamepad_input.h"
#include <string.h>

enum { TRIGGER_OFF, TRIGGER_HALF, TRIGGER_FULL };
enum { SHOULDER_NONE, SHOULDER_ZOOM, SHOULDER_FOCUS };

static bool emit(gamepad_input_t *s, pad_action_type_t type, int value)
{
    return !s->emit || s->emit(s->context, (pad_action_t){type, value, s->caps.generation});
}
static void release_all(gamepad_input_t *s)
{
    /* A dedicated release action bypasses normal command-queue capacity. */
    emit(s, PAD_ACTION_RELEASE_ALL, 0);
    emit(s, PAD_ACTION_MF_CANCEL, 0);
    s->s1 = s->s2 = s->trigger_armed = s->shoulder_armed = false;
    s->lt_stage = s->rt_stage = TRIGGER_OFF;
    s->held_shoulder = 0;
    s->held_direction = 0;
    s->direction_armed = false;
    s->select_hold = false;
    s->shoulder_mode = SHOULDER_NONE;
}
static bool send(gamepad_input_t *s, pad_action_type_t type, int value)
{
    if (emit(s, type, value)) return true;
    release_all(s);
    return false;
}
static uint8_t trigger_stage(uint8_t previous, uint8_t value)
{
    if (previous == TRIGGER_FULL && value >= 204) return TRIGGER_FULL;
    if (previous != TRIGGER_OFF && value >= 51) return value >= 230 ? TRIGGER_FULL : TRIGGER_HALF;
    if (value >= 230) return TRIGGER_FULL;
    return value >= 77 ? TRIGGER_HALF : TRIGGER_OFF;
}
static unsigned shoulder_mode(const gamepad_caps_t *caps)
{
    if (!caps->session) return SHOULDER_NONE;
    if (caps->mf_known && caps->mf && caps->lens == PAD_LENS_NON_POWER_ZOOM) return SHOULDER_FOCUS;
    if (gamepad_zoom_available(caps)) return SHOULDER_ZOOM;
    return SHOULDER_NONE;
}
static void stop_shoulders(gamepad_input_t *s)
{
    if (s->held_shoulder && s->shoulder_mode == SHOULDER_ZOOM) send(s, PAD_ACTION_ZOOM, 0);
    emit(s, PAD_ACTION_MF_CANCEL, 0);
    s->held_shoulder = 0;
    s->shoulder_mode = SHOULDER_NONE;
}
static void update_caps(gamepad_input_t *s, const gamepad_caps_t *caps)
{
    bool session_changed = caps->session != s->caps.session || caps->generation != s->caps.generation;
    bool mapping_changed = caps->mf_known != s->caps.mf_known || caps->mf != s->caps.mf ||
        caps->lens != s->caps.lens || caps->zoom_known != s->caps.zoom_known ||
        caps->zoom_enabled != s->caps.zoom_enabled;
    if (session_changed) release_all(s);
    else if (mapping_changed) { stop_shoulders(s); s->shoulder_armed = false; }
    if (caps->settings != s->caps.settings) {
        s->select_hold = false;
        s->held_direction = 0;
        s->direction_armed = false;
    }
    s->caps = *caps;
}
static void directions(gamepad_input_t *s, uint32_t buttons, bool edge, uint32_t now)
{
    uint32_t keys = buttons & PAD_DIRECTIONS;
    if (!keys) {
        s->held_direction = 0;
        s->direction_armed = s->caps.settings;
        return;
    }
    if (!s->caps.settings || (keys & (keys - 1)) ||
        (s->held_direction && keys != s->held_direction)) {
        s->held_direction = 0; s->direction_armed = false;
        return;
    }
    if (!s->direction_armed) return;
    bool move = keys == PAD_UP || keys == PAD_DOWN;
    int direction = keys == PAD_UP || keys == PAD_LEFT ? -1 : 1;
    if (!s->held_direction) {
        if (!edge) return;
        s->held_direction = keys;
        s->direction_repeat_ms = now + 400;
        send(s, move ? PAD_ACTION_MENU_MOVE : PAD_ACTION_MENU_STEP, direction);
    } else if (!edge && (int32_t)(now - s->direction_repeat_ms) >= 0) {
        s->direction_repeat_ms = now + 150;
        send(s, move ? PAD_ACTION_MENU_MOVE : PAD_ACTION_MENU_STEP, direction);
    }
}
void gamepad_input_init(gamepad_input_t *s, pad_action_fn fn, void *context)
{
    memset(s, 0, sizeof(*s));
    s->emit = fn; s->context = context;
}
void gamepad_input_online(gamepad_input_t *s, const gamepad_snapshot_t *first,
                          const gamepad_caps_t *caps)
{
    release_all(s);
    s->caps = *caps;
    s->connected = first->connected;
    s->event_buttons = first->buttons;
}
void gamepad_input_offline(gamepad_input_t *s)
{
    release_all(s);
    s->connected = false;
    s->event_buttons = 0;
}
static void shoulders(gamepad_input_t *s, uint32_t buttons, bool event, uint32_t now)
{
    uint32_t keys = buttons & PAD_SHOULDERS;
    unsigned mode = shoulder_mode(&s->caps);
    if (!keys) {
        stop_shoulders(s);
        s->shoulder_armed = s->caps.session;
        return;
    }
    if (keys == PAD_SHOULDERS || (s->held_shoulder && keys != s->held_shoulder)) {
        stop_shoulders(s); s->shoulder_armed = false;
        return;
    }
    if (!s->shoulder_armed || !mode) return;
    int direction = keys == PAD_L1 ? 1 : -1;
    if (mode == SHOULDER_ZOOM) direction = -direction; /* R1 Tele, L1 Wide; MF retains near/far. */
    if (!s->held_shoulder) {
        if (!event) return; /* Snapshots cannot synthesize a press edge. */
        s->held_shoulder = keys;
        s->shoulder_mode = (uint8_t)mode;
        s->repeat_ms = now + 400;
        send(s, mode == SHOULDER_FOCUS ? PAD_ACTION_MF_STEP : PAD_ACTION_ZOOM, direction);
    } else if (!event && mode == SHOULDER_FOCUS && (int32_t)(now - s->repeat_ms) >= 0) {
        s->repeat_ms = now + 150; /* Never catch up old missed repetitions. */
        send(s, PAD_ACTION_MF_STEP, direction);
    }
}
void gamepad_input_event(gamepad_input_t *s, uint32_t buttons, bool gap,
                         const gamepad_caps_t *caps, uint32_t now)
{
    update_caps(s, caps);
    if (!s->connected) return;
    uint32_t pressed = buttons & ~s->event_buttons;
    s->event_buttons = buttons;
    if (gap) { release_all(s); return; }
    if (!(buttons & PAD_SELECT) || caps->session || caps->settings) s->select_hold=false;
    else if (pressed & PAD_SELECT) { s->select_hold=true;s->select_deadline=now+2000; }
    if ((pressed & PAD_START) && !send(s, PAD_ACTION_UI_TOGGLE, 1)) return;
    if ((pressed & PAD_TOUCH) && !send(s,PAD_ACTION_UI_INFO_NEXT,1)) return;
    if (caps->settings) {
        if (pressed & (PAD_A | PAD_B)) { s->held_direction = 0; s->direction_armed = false; }
        if ((pressed & PAD_B) && !send(s, PAD_ACTION_MENU_BACK, 1)) return;
        else if ((pressed & PAD_A) && !send(s, PAD_ACTION_MENU_CONFIRM, 1)) return;
    }
    if (caps->session) {
        if ((pressed & PAD_Y) && !send(s, PAD_ACTION_MODE_NEXT, 1)) return;
        if (pressed & PAD_X) {
            stop_shoulders(s);
            s->shoulder_armed = false;
            if (!send(s, PAD_ACTION_FOCUS_MODE_NEXT, 1)) return;
        }
    }
    shoulders(s, buttons, (pressed & PAD_SHOULDERS) != 0, now);
    directions(s, buttons, (pressed & PAD_DIRECTIONS) != 0, now);
}
void gamepad_input_snapshot(gamepad_input_t *s, const gamepad_snapshot_t *p,
                            const gamepad_caps_t *caps, uint32_t now)
{
    update_caps(s, caps);
    if (!p->connected) { if (s->connected) gamepad_input_offline(s); return; }
    if (!s->connected) gamepad_input_online(s, p, caps);
    if (caps->session || caps->settings || !(p->buttons & s->event_buttons & PAD_SELECT)) s->select_hold=false;
    if (s->select_hold && (int32_t)(now-s->select_deadline)>=0) {
        s->select_hold=false;
        if (!send(s,PAD_ACTION_MAINT_TOGGLE,1)) return;
    }
    unsigned old_lt = s->lt_stage;
    s->lt_stage = trigger_stage(s->lt_stage, p->lt);
    s->rt_stage = trigger_stage(s->rt_stage, p->rt);
    if (!caps->session) {
        shoulders(s, p->buttons, false, now);
        directions(s, p->buttons, false, now);
        return;
    }
    if (!s->trigger_armed) {
        if (p->lt < 51 && p->rt < 51) s->trigger_armed = true;
    } else {
        bool s1 = s->lt_stage != TRIGGER_OFF || s->rt_stage != TRIGGER_OFF;
        bool s2 = s->rt_stage == TRIGGER_FULL;
        if (s->s2 && !s2 && !send(s, PAD_ACTION_S2, 0)) return;
        if (s->s1 && !s1 && !send(s, PAD_ACTION_S1, 0)) return;
        if (!s->s1 && s1 && !send(s, PAD_ACTION_S1, 1)) return;
        if (!s->s2 && s2 && !send(s, PAD_ACTION_S2, 1)) return;
        s->s1 = s1; s->s2 = s2;
        if (old_lt != TRIGGER_FULL && s->lt_stage == TRIGGER_FULL) {
            if (caps->recording_known && !caps->record_pending) {
                if (!send(s, PAD_ACTION_RECORD, !caps->recording)) return;
            } else send(s, PAD_ACTION_RECORD_UNAVAILABLE, 0);
        }
    }
    shoulders(s, p->buttons, false, now);
    directions(s, p->buttons, false, now);
}
