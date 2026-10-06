#include "camera_stream.h"
#include "camera_settings_execute.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_log.h"
#define STREAM_STAGE(name) stage=(name)
#else
#define STREAM_STAGE(name) ((void)0)
#endif
static void enter(camera_stream_t *s) { if (s->controls.enter) s->controls.enter(s->controls.guard_context); }
static void leave(camera_stream_t *s) { if (s->controls.leave) s->controls.leave(s->controls.guard_context); }
void camera_stream_focus_cancel(camera_stream_t *s) { if (s) atomic_fetch_add(&s->focus_epoch,1); }
static gamepad_caps_t caps(camera_stream_t *s)
{
    enter(s); gamepad_caps_t value=s->controls.caps; leave(s); return value;
}
static bool pending(camera_stream_t *s)
{
    enter(s); bool value=s->controls.actions.count || s->controls.actions.release_pending; leave(s); return value;
}
static void cancel_settings(camera_stream_t *s)
{
    if (s->mode.dirty || s->mode.awaiting) setting_control_response(&s->mode,false);
    camera_menu_cancel(&s->menu);
    s->mode_steps=0; memset(s->menu_steps,0,sizeof s->menu_steps); s->publish=true;
    s->focus_pending=false;
}
static void synchronize(camera_stream_t *s)
{
    uint32_t next=caps(s).generation;
    if (next!=s->safety_generation) { cancel_settings(s); s->safety_generation=next; }
}
static void publish(camera_stream_t *s)
{
    if (!s->publish && !s->controls.changed) return;
    gamepad_caps_t value=caps(s); app_camera_view_t view;
    if (camera_view_build(&s->properties,&s->mode,&s->menu,&value,&view) &&
        camera_outputs_properties(s->generation,&view)==ESP_OK) s->publish=false;
    for (unsigned i=0;i<APP_CAMERA_CONTROL_COUNT;++i) {
        if ((s->controls.changed & (1u<<i)) &&
            camera_outputs_command(s->generation,i,s->controls.status[i])==ESP_OK)
            s->controls.changed &= ~(1u<<i);
    }
}
bool camera_stream_begin(camera_stream_t *s,uint32_t generation,
    uint8_t *first,uint8_t *second,size_t capacity,pad_lens_t lens,uint32_t now)
{
    if (!s || s->running || (!!s->controls.enter!=!!s->controls.leave) ||
        (s->frames.generation && !camera_frames_drained(&s->frames))) return false;
    if (!camera_frames_init(&s->frames,generation,first,second,capacity)) return false;
    s->properties=(camera_properties_t){0}; s->mode=(setting_control_t){0}; s->menu=(camera_menu_t){0};
    s->generation=generation; s->next_read=s->retry_at=now; s->refused=0;
    s->mode_steps=0; memset(s->menu_steps,0,sizeof s->menu_steps);
    s->focus_pending=false; s->properties_at=now;
    camera_controls_session(&s->controls,true,lens);
    s->safety_generation=caps(s).generation; s->running=s->refresh=s->publish=true;
    return true;
}
static bool adjust(camera_stream_t *s,unsigned property,int direction)
{
    if (direction!=1 && direction!=-1) return false;
    int *steps=NULL;
    if (property==APP_CAMERA_PROPERTY_MODE) steps=&s->mode_steps;
    for (unsigned i=0;i<CAMERA_MENU_COUNT;++i)
        if (property==camera_menu_properties[i]) steps=&s->menu_steps[i];
    if (!steps) return false;
    if (property==APP_CAMERA_PROPERTY_MODE || property==APP_CAMERA_PROPERTY_FOCUS) s->focus_pending=false;
    if (*steps>-1000000 && *steps<1000000) *steps+=direction;
    s->refresh=true; return true;
}
esp_err_t camera_stream_message(camera_stream_t *s,const app_message_t *message,uint32_t now)
{
    if (!s || !message) return ESP_ERR_INVALID_ARG;
    if (message->type==APP_MESSAGE_UI_FRAME_RESULT)
        return camera_frames_result(&s->frames,message) ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (!s->running || message->lease) return ESP_ERR_INVALID_STATE;
    synchronize(s);
    if (message->type==APP_MESSAGE_CAMERA_SETTING_ADJUST) {
        if (message->payload.command.token && message->payload.command.token!=caps(s).generation)
            return ESP_ERR_INVALID_STATE;
        return adjust(s,message->payload.command.index,message->payload.command.direction) ? ESP_OK : ESP_ERR_INVALID_ARG;
    }
    if (message->type==APP_MESSAGE_CAMERA_ACTION) {
        pad_action_t action=message->payload.action;
        if (action.type==PAD_ACTION_MF_CANCEL) { camera_stream_focus_cancel(s); s->focus_pending=false; return ESP_OK; }
        if (action.generation!=caps(s).generation) return ESP_ERR_INVALID_STATE;
        if (action.type==PAD_ACTION_MF_STEP) {
            gamepad_caps_t value=caps(s);
            if ((action.value!=1 && action.value!=-1) || !value.mf_known || !value.mf ||
                value.lens!=PAD_LENS_NON_POWER_ZOOM || (uint32_t)(now-s->properties_at)>=6000)
                return ESP_ERR_INVALID_STATE;
            s->focus_pending=true; s->focus_direction=action.value;
            s->focus_at=now; s->focus_generation=action.generation;
            s->focus_request_epoch=atomic_load(&s->focus_epoch); s->refresh=true; return ESP_OK;
        }
        if (action.type==PAD_ACTION_MODE_NEXT || action.type==PAD_ACTION_FOCUS_MODE_NEXT)
            return adjust(s,action.type==PAD_ACTION_MODE_NEXT ? APP_CAMERA_PROPERTY_MODE : APP_CAMERA_PROPERTY_FOCUS,1) ? ESP_OK : ESP_ERR_INVALID_ARG;
        bool accepted=camera_controls_submit(&s->controls,action,now);
        synchronize(s); s->publish=true;
        return accepted ? ESP_OK : ESP_ERR_INVALID_STATE;
    }
    return ESP_ERR_NOT_SUPPORTED;
}
static camera_backend_result_t controls(camera_stream_t *s,camera_backend_t *backend,
    void *scratch,size_t capacity,uint32_t timeout,uint32_t (*now)(void *),void *context)
{
    uint32_t started=now(context);
    for (unsigned i=0;i<CAMERA_ACTION_CAPACITY+3 && pending(s);++i) {
        uint32_t elapsed=now(context)-started;
        if (elapsed>=timeout) return CAMERA_BACKEND_TIMEOUT;
        enter(s); size_t before=s->controls.actions.count;
        uint32_t generation=s->controls.actions.generation;
        bool s1=s->controls.actions.s1, s2=s->controls.actions.s2;
        int zoom=s->controls.actions.zoom; leave(s);
        camera_backend_result_t result=camera_controls_tick(&s->controls,backend,&s->properties,
            &s->mode,&s->menu,scratch,capacity,timeout-elapsed,now,context);
        synchronize(s); s->publish=true;
        if (result!=CAMERA_BACKEND_OK) return result;
        enter(s);
        if (!s->controls.actions.count && !s->controls.actions.s1 && !s->controls.actions.s2 && !s->controls.actions.zoom)
            s->controls.actions.release_pending=false;
        bool unchanged=before==s->controls.actions.count && generation==s->controls.actions.generation &&
            s1==s->controls.actions.s1 && s2==s->controls.actions.s2 && zoom==s->controls.actions.zoom; leave(s);
        if (unchanged) break; /* Deferred property-dependent press: do not busy-spin. */
    }
    return CAMERA_BACKEND_OK;
}
camera_backend_result_t camera_stream_tick(camera_stream_t *s,camera_backend_t *backend,
    uint32_t timeout,uint32_t (*now)(void *),void *context)
{
    if (!s || !backend || !timeout || !now) return CAMERA_BACKEND_INVALID;
    if (!s->running || s->frames.failed) return CAMERA_BACKEND_STATE;
    const uint32_t required=CAMERA_BACKEND_CAP_PROPERTIES|CAMERA_BACKEND_CAP_ACTIONS|
        CAMERA_BACKEND_CAP_LIVEVIEW|CAMERA_BACKEND_CAP_EVENTS;
    if (backend->api_version!=CAMERA_BACKEND_API_VERSION || !backend->ops ||
        backend->ops->api_version!=CAMERA_BACKEND_API_VERSION || (backend->capabilities&required)!=required ||
        !backend->ops->events || !backend->ops->liveview) return CAMERA_BACKEND_UNSUPPORTED;
    synchronize(s);
#ifdef ESP_PLATFORM
    const char *stage="controls-before";
    uint32_t tick_started=now(context);
#endif
    int slot=camera_frames_acquire(&s->frames);
    void *scratch=slot<0 ? NULL : s->frames.slots[slot].buffer;
    size_t capacity=slot<0 ? 0 : s->frames.slots[slot].capacity;
    camera_backend_result_t result=controls(s,backend,scratch,capacity,timeout,now,context);
    if (result!=CAMERA_BACKEND_OK) goto done;
    if (slot<0) { publish(s); return CAMERA_BACKEND_OK; }
    bool changed=false;
    STREAM_STAGE("events");
    result=backend->ops->events(backend->context,timeout,&changed);
    if (result!=CAMERA_BACKEND_OK) goto done;
    bool fresh=false;
    if (s->focus_pending && (uint32_t)(now(context)-s->focus_at)>=500) s->focus_pending=false;
    if (s->refresh || changed || (int32_t)(now(context)-s->next_read)>=0) {
        STREAM_STAGE("properties");
        result=camera_properties_read(backend,scratch,capacity,timeout,&s->properties);
        camera_properties_apply(&s->properties,&s->mode,&s->menu,now(context));
        s->publish=true;
        if (result!=CAMERA_BACKEND_OK) goto done;
        camera_controls_observe(&s->controls,&s->properties.capabilities,now(context));
        s->properties_at=now(context);
        synchronize(s);
        if (s->mode_steps) setting_control_step(&s->mode,s->mode_steps);
        for (unsigned i=0;i<CAMERA_MENU_COUNT;++i)
            if (s->menu_steps[i]) camera_menu_step(&s->menu,i,s->menu_steps[i],true);
        s->mode_steps=0; memset(s->menu_steps,0,sizeof s->menu_steps);
        s->refresh=false; s->publish=true; fresh=true; s->next_read=now(context)+5000;
    }
    STREAM_STAGE("controls-after-properties");
    result=controls(s,backend,scratch,capacity,timeout,now,context);
    if (result!=CAMERA_BACKEND_OK) goto done;
    bool issued=false;
    synchronize(s); /* Intake release may have changed safety generation during IO. */
    STREAM_STAGE("settings");
    result=camera_settings_execute(backend,&s->properties,&s->mode,&s->menu,&fresh,now(context),timeout,&issued);
    if (result!=CAMERA_BACKEND_OK) goto done;
    if (issued) s->publish=true;
    if (s->focus_pending) {
        gamepad_caps_t value=caps(s); s->focus_pending=false;
        if (s->focus_request_epoch==atomic_load(&s->focus_epoch) && value.generation==s->focus_generation && value.mf_known && value.mf &&
            value.lens==PAD_LENS_NON_POWER_ZOOM && (uint32_t)(now(context)-s->focus_at)<1000) {
            result=backend->ops->action(backend->context,CAMERA_ACTION_FOCUS_STEP,s->focus_direction,timeout);
            s->controls.status[APP_CAMERA_CONTROL_FOCUS_STEP]=result==CAMERA_BACKEND_OK ? SETTING_ACCEPTED : SETTING_REJECTED;
            s->controls.changed |= 1u<<APP_CAMERA_CONTROL_FOCUS_STEP;
            if (result==CAMERA_BACKEND_REFUSED) {
                enter(s); s->controls.caps.mf_known=false; leave(s); result=CAMERA_BACKEND_OK;
            }
            if (result!=CAMERA_BACKEND_OK) goto done;
        }
    }
    if (s->mode.awaiting || camera_menu_pending(&s->menu) || caps(s).record_pending) {
        uint32_t sooner=now(context)+500;
        if ((int32_t)(s->next_read-sooner)>0) s->next_read=sooner;
    }
    STREAM_STAGE("controls-before-frame");
    result=controls(s,backend,scratch,capacity,timeout,now,context);
    if (result!=CAMERA_BACKEND_OK || pending(s) || (int32_t)(now(context)-s->retry_at)<0) goto done;
    uint32_t entered=now(context); camera_frame_t frame={0};
    STREAM_STAGE("liveview");
    result=backend->ops->liveview(backend->context,scratch,capacity,timeout,&frame);
    if (result==CAMERA_BACKEND_NOT_READY && ++s->refused<=50) {
        s->retry_at=now(context)+100; result=CAMERA_BACKEND_OK;
    } else if (result==CAMERA_BACKEND_DROPPED) {
        s->refused=0; camera_frames_drop(&s->frames,slot); slot=-1; result=CAMERA_BACKEND_OK;
    } else if (result==CAMERA_BACKEND_OK) {
        s->refused=0;
        esp_err_t sent=camera_frames_publish(&s->frames,slot,frame.jpeg,frame.size,now(context)-entered);
        if (sent==ESP_OK || sent==ESP_ERR_TIMEOUT) slot=-1;
        else result=sent==ESP_ERR_NO_MEM ? CAMERA_BACKEND_NO_MEMORY : CAMERA_BACKEND_INVALID;
    }
done:
#ifdef ESP_PLATFORM
    if (result!=CAMERA_BACKEND_OK)
        ESP_LOGW("camera_pair","Stream stage=%s backend=%d elapsed=%lums slot=%d",stage,result,(unsigned long)(now(context)-tick_started),slot);
#endif
    if (slot>=0) camera_frames_discard(&s->frames,slot);
    publish(s);
    return result;
}
void camera_stream_stop(camera_stream_t *s,uint32_t now)
{
    if (!s) return;
    s->running=false; cancel_settings(s);
    camera_controls_submit(&s->controls,(pad_action_t){PAD_ACTION_RELEASE_ALL,0,caps(s).generation},now);
    synchronize(s);
}
camera_backend_result_t camera_stream_release(camera_stream_t *s,camera_backend_t *backend,
    uint32_t timeout,uint32_t (*now)(void *),void *context)
{
    if (!s || s->running || !backend || !now || !timeout) return CAMERA_BACKEND_INVALID;
    camera_backend_result_t result=controls(s,backend,NULL,0,timeout,now,context);
    publish(s); return result;
}
bool camera_stream_end(camera_stream_t *s)
{
    if (!s || s->running || !camera_frames_drained(&s->frames)) return false;
    camera_controls_session(&s->controls,false,caps(s).lens);
    s->properties.valid=false;
    camera_properties_apply(&s->properties,&s->mode,&s->menu,0);
    s->publish=true; publish(s); return true;
}
