#include "input_owner.h"
#include <string.h>

static input_report_source_t source(input_source_kind_t kind)
{ return kind==INPUT_SOURCE_ATOM ? INPUT_REPORT_ATOM : INPUT_REPORT_SIM; }
static void clear_status(input_owner_t *s)
{ s->latest=(input_report_t){.battery=255}; }
void input_owner_init(input_owner_t *s, pad_action_fn emit, void *context)
{
    if (!s) return;
    memset(s,0,sizeof(*s)); clear_status(s);
    input_reports_init(&s->reports,emit,context);
    s->selected=INPUT_SOURCE_ATOM;
    input_reports_select(&s->reports,INPUT_REPORT_ATOM);
}
bool input_owner_select(input_owner_t *s, input_source_kind_t kind)
{
    if (!s || kind<INPUT_SOURCE_ATOM || kind>=INPUT_SOURCE_COUNT) return false;
    if (s->selected!=kind) { s->selected=kind; clear_status(s); }
    return input_reports_select(&s->reports,source(kind));
}
void input_owner_tick(input_owner_t *s, const gamepad_caps_t *caps, uint32_t now)
{
    if (!s || !caps) return;
    input_reports_retry_release(&s->reports);
    input_provider_event_t event;
    /* Bounded even if a provider is publishing concurrently. Safety events
     * are yielded ahead of the ordinary queue by the registry. */
    for (unsigned i=0;i<INPUT_PROVIDER_REPORT_CAPACITY+INPUT_SOURCE_COUNT &&
        input_provider_registry_next(&event);++i) {
        input_report_source_t kind=source(event.source);
        if (event.disconnected) {
            if (!s->handles[event.source] || s->handles[event.source]==event.handle) {
                input_reports_disconnect(&s->reports,kind);
                if (event.source==s->selected) clear_status(s);
            }
            continue;
        }
        if (s->handles[event.source]!=event.handle) {
            input_reports_disconnect(&s->reports,kind);
            s->reports.sources[kind]=(input_report_cursor_t){0};
            s->handles[event.source]=event.handle;
        }
        /* The provider can enqueue while an action callback blocks during this
         * tick. A capture later than sampled now has zero age, not UINT32_MAX
         * age. Signed ordering also handles the monotonic ms counter wrapping. */
        if ((int32_t)(now-event.captured_ms)>1000) {
            input_reports_disconnect(&s->reports,kind);
            if (event.source==s->selected) clear_status(s);
            continue;
        }
        const input_report_t *r=&event.report;
        input_report_frame_t frame={.snapshot={.connected=r->connected,.buttons=r->buttons,
            .rx=(int8_t)r->rx,.ry=(int8_t)r->ry,.lt=r->lt,.rt=r->rt,.battery=r->battery},
            .source_epoch=r->source_epoch,.report_id=r->report_id,.gap=r->gap,
            .event_valid=r->event_valid,.event_buttons=r->event_buttons};
        if (input_reports_publish(&s->reports,kind,&frame,caps,now)) s->latest=*r;
        else if (event.source==s->selected && !s->reports.gamepad.connected) clear_status(s);
    }
}
bool input_owner_quiesce(input_owner_t *s)
{
    if (!s) return false;
    input_provider_registry_close(); clear_status(s);
    return input_reports_select(&s->reports,INPUT_REPORT_NONE);
}
