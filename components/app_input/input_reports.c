#include "input_reports.h"
#include <string.h>

static bool emit(void *context, pad_action_t action)
{
    input_reports_t *s = context;
    bool safety = action.type == PAD_ACTION_RELEASE_ALL || action.type == PAD_ACTION_MF_CANCEL;
    if (!safety && (s->release_pending || s->mf_pending)) return false;
    bool ok = s->emit && s->emit(s->context, action);
    if (action.type == PAD_ACTION_RELEASE_ALL) s->release_pending = !ok;
    if (action.type == PAD_ACTION_MF_CANCEL) s->mf_pending = !ok;
    return ok;
}

bool input_reports_retry_release(input_reports_t *s)
{
    if (!s) return false;
    if (s->release_pending) emit(s, (pad_action_t){PAD_ACTION_RELEASE_ALL, 0, s->gamepad.caps.generation});
    if (s->mf_pending) emit(s, (pad_action_t){PAD_ACTION_MF_CANCEL, 0, s->gamepad.caps.generation});
    return !s->release_pending && !s->mf_pending;
}

static void release(input_reports_t *s)
{
    gamepad_input_offline(&s->gamepad);
    s->baseline = true;
}

void input_reports_init(input_reports_t *s, pad_action_fn sink, void *context)
{
    if (!s) return;
    memset(s, 0, sizeof(*s)); s->emit = sink; s->context = context; s->baseline = true;
    gamepad_input_init(&s->gamepad, emit, s);
}

bool input_reports_select(input_reports_t *s, input_report_source_t source)
{
    if (!s || source < INPUT_REPORT_NONE || source >= INPUT_REPORT_SOURCE_COUNT) return false;
    if (s->selected != source) { release(s); s->selected = source; }
    return input_reports_retry_release(s);
}

void input_reports_disconnect(input_reports_t *s, input_report_source_t source)
{
    if (s && source != INPUT_REPORT_NONE && source == s->selected) release(s);
}

bool input_reports_publish(input_reports_t *s, input_report_source_t source,
    const input_report_frame_t *r, const gamepad_caps_t *caps, uint32_t now)
{
    if (!s || !r || !caps || source <= INPUT_REPORT_NONE || source >= INPUT_REPORT_SOURCE_COUNT ||
        source != s->selected || !r->source_epoch || !r->report_id ||
        (r->snapshot.battery > 100 && r->snapshot.battery != 255)) return false;
    input_report_cursor_t *cursor = &s->sources[source];
    if (cursor->known && (r->source_epoch < cursor->epoch ||
        (r->source_epoch == cursor->epoch && r->report_id == cursor->id))) return false;
    if (cursor->known && r->source_epoch == cursor->epoch) {
        if (cursor->quarantined) return false;
        if (r->report_id < cursor->id) { cursor->quarantined = true; release(s); return false; }
    } else {
        release(s); *cursor = (input_report_cursor_t){.epoch=r->source_epoch, .known=true};
    }
    if (!input_reports_retry_release(s)) return false;
    cursor->id = r->report_id;
    if (!r->snapshot.connected) { release(s); return input_reports_retry_release(s); }
    if (s->baseline || !s->gamepad.connected) {
        gamepad_input_online(&s->gamepad, &r->snapshot, caps);
        s->baseline = false;
        /* Cached press edges cannot survive reconnect or source selection. */
        if (s->release_pending || s->mf_pending) { release(s); return false; }
    } else if (r->gap) {
        gamepad_input_event(&s->gamepad, r->snapshot.buttons, true, caps, now);
    } else if (r->event_valid) {
        gamepad_input_event(&s->gamepad, r->event_buttons, false, caps, now);
    }
    if (s->release_pending || s->mf_pending) { release(s); return false; }
    gamepad_input_snapshot(&s->gamepad, &r->snapshot, caps, now);
    if (s->release_pending || s->mf_pending) { release(s); return false; }
    return true;
}
