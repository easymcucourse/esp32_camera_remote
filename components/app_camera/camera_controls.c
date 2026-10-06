#include "camera_controls.h"
static void enter(camera_controls_t *c) { if (c->enter) c->enter(c->guard_context); }
static void leave(camera_controls_t *c) { if (c->leave) c->leave(c->guard_context); }

static void status(camera_controls_t *c, app_camera_control_t id, setting_status_t value)
{
    if (c->status[id] != value) {
        c->status[id] = value;
        c->changed |= 1u << id;
    }
}
static void synchronize(camera_controls_t *c)
{
    c->caps.session = c->actions.session;
    c->caps.generation = c->actions.generation;
    c->caps.record_pending = c->record_wait || c->record_executing || camera_actions_record_queued(&c->actions);
}
void camera_controls_session(camera_controls_t *c, bool open, pad_lens_t lens)
{
    enter(c);
    camera_actions_session(&c->actions, open);
    c->record_wait = c->record_executing = false;
    c->caps = (gamepad_caps_t){.lens = lens};
    for (unsigned i = 0; i < APP_CAMERA_CONTROL_COUNT; ++i) c->status[i] = SETTING_IDLE;
    c->changed = (1u << APP_CAMERA_CONTROL_COUNT) - 1u;
    synchronize(c);
    leave(c);
}
bool camera_controls_submit(camera_controls_t *c, pad_action_t action, uint32_t now)
{
    if (!c) return false;
    enter(c);
    if (action.type == PAD_ACTION_RECORD &&
        (c->record_wait || c->record_executing || camera_actions_record_queued(&c->actions))) { leave(c); return false; }
    bool result = camera_actions_submit(&c->actions, action, now);
    synchronize(c);
    leave(c);
    return result;
}
void camera_controls_observe(camera_controls_t *c, const camera_capabilities_t *caps, uint32_t now)
{
    if (!c || !caps) return;
    enter(c);
    c->caps.mf_known = caps->focus_known;
    c->caps.mf = caps->manual_focus;
    c->caps.zoom_known = caps->zoom_known;
    c->caps.zoom_enabled = caps->zoom_enabled;
    c->caps.recording_known = caps->recording_known;
    c->caps.recording = caps->recording;
    if (c->record_wait) {
        if (caps->recording_known && caps->recording == c->record_target) {
            c->record_wait = false;
            status(c, APP_CAMERA_CONTROL_RECORD, SETTING_APPLIED);
        } else if ((int32_t)(now - c->record_deadline) >= 0) {
            c->record_wait = false;
            status(c, APP_CAMERA_CONTROL_RECORD, SETTING_TIMEOUT);
        }
    }
    synchronize(c);
    leave(c);
}
static void release_all(camera_controls_t *c, uint32_t now)
{
    camera_actions_submit(&c->actions,
        (pad_action_t){PAD_ACTION_RELEASE_ALL, 0, c->actions.generation}, now);
    synchronize(c);
}
static void execution_finished(camera_controls_t *c)
{
    enter(c); c->record_executing=false; synchronize(c); leave(c);
}
camera_backend_result_t camera_controls_tick(camera_controls_t *c,
    camera_backend_t *backend, camera_properties_t *properties,
    setting_control_t *mode, camera_menu_t *menu, void *scratch, size_t capacity,
    uint32_t timeout_ms, uint32_t (*now_ms)(void *), void *clock_context)
{
    if (!c || !backend || !properties || !mode || !menu || (!!scratch != !!capacity) ||
        !timeout_ms || !now_ms || (!!c->enter != !!c->leave)) return CAMERA_BACKEND_INVALID;
    if (backend->api_version != CAMERA_BACKEND_API_VERSION || !backend->ops ||
        !(backend->capabilities & CAMERA_BACKEND_CAP_ACTIONS) || !backend->ops->action)
        return CAMERA_BACKEND_UNSUPPORTED;
    uint32_t started = now_ms(clock_context);
    camera_queued_action_t action;
    enter(c);
    bool found=scratch ? camera_actions_next(&c->actions, now_ms(clock_context), &action) :
        camera_actions_next_direct(&c->actions,now_ms(clock_context),&action);
    if (!found) {
        synchronize(c); leave(c); return CAMERA_BACKEND_OK;
    }
    c->record_executing = action.action.type == PAD_ACTION_RECORD;
    synchronize(c);
    leave(c);
    pad_action_type_t type = action.action.type;
    app_camera_control_t id;
    camera_action_t operation;
    switch (type) {
    case PAD_ACTION_S1: id = APP_CAMERA_CONTROL_SHUTTER_HALF; operation = CAMERA_ACTION_SHUTTER_HALF; break;
    case PAD_ACTION_S2: id = APP_CAMERA_CONTROL_SHUTTER_FULL; operation = CAMERA_ACTION_SHUTTER_FULL; break;
    case PAD_ACTION_ZOOM: id = APP_CAMERA_CONTROL_ZOOM; operation = CAMERA_ACTION_ZOOM; break;
    case PAD_ACTION_RECORD: id = APP_CAMERA_CONTROL_RECORD; operation = CAMERA_ACTION_RECORD; break;
    default: return CAMERA_BACKEND_INVALID;
    }
    if (type == PAD_ACTION_RECORD || (type == PAD_ACTION_ZOOM && action.action.value)) {
        camera_backend_result_t result = camera_properties_read(backend, scratch, capacity,
            timeout_ms, properties);
        camera_properties_apply(properties, mode, menu, now_ms(clock_context));
        if (result != CAMERA_BACKEND_OK) { execution_finished(c); return result; }
        camera_controls_observe(c, &properties->capabilities, now_ms(clock_context));
        enter(c);
        if (!camera_actions_current(&c->actions, &action)) {
            c->record_executing=false; synchronize(c); leave(c); return CAMERA_BACKEND_OK;
        }
        bool eligible = type == PAD_ACTION_RECORD ? c->caps.recording_known && !c->record_wait &&
            c->caps.recording != (action.action.value != 0) : gamepad_zoom_available(&c->caps) &&
            !(c->caps.mf_known && c->caps.mf && c->caps.lens == PAD_LENS_NON_POWER_ZOOM);
        if (!eligible) {
            c->record_executing=false;
            status(c, id, SETTING_REJECTED);
            release_all(c, now_ms(clock_context));
            leave(c);
            return CAMERA_BACKEND_OK;
        }
        leave(c);
    }
    enter(c);
    bool current = camera_actions_current(&c->actions, &action);
    leave(c);
    if (!current) { execution_finished(c); return CAMERA_BACKEND_OK; }
    uint32_t elapsed = (uint32_t)(now_ms(clock_context) - started);
    if (elapsed >= timeout_ms) { execution_finished(c); return CAMERA_BACKEND_TIMEOUT; }
    camera_backend_result_t result = backend->ops->action(backend->context, operation,
        action.action.value, timeout_ms - elapsed);
    enter(c);
    c->record_executing=false;
    camera_actions_complete(&c->actions, &action, result == CAMERA_BACKEND_OK);
    if (result != CAMERA_BACKEND_OK && result != CAMERA_BACKEND_REFUSED) {
        synchronize(c); leave(c); return result;
    }
    status(c, id, result == CAMERA_BACKEND_OK ? SETTING_ACCEPTED : SETTING_REJECTED);
    if (result == CAMERA_BACKEND_REFUSED) {
        if (action.release) { leave(c); return result; }
        release_all(c, now_ms(clock_context));
    } else if (type == PAD_ACTION_RECORD) {
        c->record_wait = true;
        c->record_target = action.action.value != 0;
        c->record_deadline = now_ms(clock_context) + 10000;
        status(c, id, SETTING_PENDING);
    }
    synchronize(c);
    leave(c);
    return CAMERA_BACKEND_OK;
}
