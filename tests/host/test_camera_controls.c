#include "camera_controls.h"
#include <assert.h>
#include <string.h>
static camera_controls_t controls;
static camera_properties_t snapshot;
static setting_control_t mode;
static camera_menu_t menu;
static unsigned reads, writes;
static uint32_t clock_ms, read_delay, last_timeout;
static bool cancel_during_read;
static camera_capabilities_t capabilities;
static camera_backend_result_t read_result, action_result;
static camera_action_t last_operation;
static int last_value;
static unsigned guard_depth, enters, leaves;
static void enter_guard(void *context) { assert(context==&guard_depth && !guard_depth); ++guard_depth; ++enters; }
static void leave_guard(void *context) { assert(context==&guard_depth && guard_depth==1); --guard_depth; ++leaves; }
static uint32_t now(void *context) { (void)context; return clock_ms; }
static camera_backend_result_t properties(void *context, void *scratch, size_t capacity,
    uint32_t timeout, camera_property_visitor_t visitor, void *visitor_context,
    camera_capabilities_t *caps)
{
    (void)context; (void)visitor; (void)visitor_context;
    assert(scratch && capacity && timeout && !guard_depth); ++reads;
    if (controls.record_executing) {
        assert(controls.caps.record_pending);
        assert(!camera_controls_submit(&controls,
            (pad_action_t){PAD_ACTION_RECORD,1,controls.actions.generation},clock_ms));
    }
    clock_ms += read_delay; *caps = capabilities;
    if (cancel_during_read) camera_controls_submit(&controls,
        (pad_action_t){PAD_ACTION_RELEASE_ALL,0,controls.actions.generation}, clock_ms);
    return read_result;
}
static camera_backend_result_t action(void *context, camera_action_t operation, int value, uint32_t timeout)
{
    (void)context; assert(!guard_depth); ++writes; last_operation = operation; last_value = value; last_timeout = timeout;
    if (operation==CAMERA_ACTION_RECORD) {
        assert(controls.caps.record_pending && controls.record_executing);
        assert(!camera_controls_submit(&controls,
            (pad_action_t){PAD_ACTION_RECORD,0,controls.actions.generation},clock_ms));
    }
    return action_result;
}
static const camera_backend_ops_t ops = {.properties=properties,.action=action};
static camera_backend_t backend = {.api_version=CAMERA_BACKEND_API_VERSION,
    .capabilities=CAMERA_BACKEND_CAP_PROPERTIES|CAMERA_BACKEND_CAP_ACTIONS,.ops=&ops};
static void reset(pad_lens_t lens)
{
    memset(&controls,0,sizeof controls); memset(&mode,0,sizeof mode); memset(&menu,0,sizeof menu);
    assert(!guard_depth && enters==leaves);
    controls.enter=enter_guard; controls.leave=leave_guard; controls.guard_context=&guard_depth;
    clock_ms=read_delay=reads=writes=0; cancel_during_read=false;
    capabilities=(camera_capabilities_t){.recording_known=true};
    read_result=action_result=CAMERA_BACKEND_OK;
    camera_controls_session(&controls,true,lens);
}
static bool submit(pad_action_type_t type, int value)
{ return camera_controls_submit(&controls,(pad_action_t){type,value,controls.actions.generation},clock_ms); }
static camera_backend_result_t tick(void)
{
    unsigned char scratch[32];
    return camera_controls_tick(&controls,&backend,&snapshot,&mode,&menu,scratch,sizeof scratch,500,now,NULL);
}
static camera_backend_result_t direct(void)
{ return camera_controls_tick(&controls,&backend,&snapshot,&mode,&menu,NULL,0,500,now,NULL); }
int main(void)
{
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_S1,1) && tick()==CAMERA_BACKEND_OK);
    assert(!reads && writes==1 && last_operation==CAMERA_ACTION_SHUTTER_HALF && controls.actions.s1);
    assert(submit(PAD_ACTION_S2,1) && tick()==CAMERA_BACKEND_OK && controls.actions.s2);
    assert(submit(PAD_ACTION_ZOOM,-1) && tick()==CAMERA_BACKEND_OK);
    assert(reads==1 && last_operation==CAMERA_ACTION_ZOOM && last_value==-1 && controls.actions.zoom==-1);
    assert(submit(PAD_ACTION_RELEASE_ALL,0));
    assert(tick()==CAMERA_BACKEND_OK && last_operation==CAMERA_ACTION_SHUTTER_FULL && last_value==0);
    assert(tick()==CAMERA_BACKEND_OK && last_operation==CAMERA_ACTION_SHUTTER_HALF && last_value==0);
    assert(tick()==CAMERA_BACKEND_OK && last_operation==CAMERA_ACTION_ZOOM && last_value==0 && reads==1);
    assert(!controls.actions.s1 && !controls.actions.s2 && !controls.actions.zoom);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_RECORD,1) && !submit(PAD_ACTION_RECORD,1));
    read_delay=20;
    assert(tick()==CAMERA_BACKEND_OK && last_timeout==480 && controls.record_wait && controls.caps.record_pending);
    assert(controls.status[APP_CAMERA_CONTROL_RECORD]==SETTING_PENDING && controls.record_deadline==10020);
    assert(!submit(PAD_ACTION_RECORD,0));
    camera_controls_observe(&controls,&capabilities,300);
    assert(controls.record_wait); capabilities.recording=true;
    camera_controls_observe(&controls,&capabilities,400);
    assert(!controls.record_wait && !controls.caps.record_pending && controls.status[APP_CAMERA_CONTROL_RECORD]==SETTING_APPLIED);
    assert(submit(PAD_ACTION_RECORD,0) && tick()==CAMERA_BACKEND_OK);
    clock_ms=controls.record_deadline;
    camera_controls_observe(&controls,&capabilities,clock_ms);
    assert(!controls.record_wait && controls.status[APP_CAMERA_CONTROL_RECORD]==SETTING_TIMEOUT);
    reset(PAD_LENS_UNKNOWN); capabilities.recording_known=false;
    assert(submit(PAD_ACTION_RECORD,1) && tick()==CAMERA_BACKEND_OK && !writes);
    assert(controls.status[APP_CAMERA_CONTROL_RECORD]==SETTING_REJECTED);
    assert(submit(PAD_ACTION_ZOOM,1) && tick()==CAMERA_BACKEND_OK && !writes);
    /* A rejected zoom press was marked potentially latched before refreshing. */
    assert(tick()==CAMERA_BACKEND_OK && writes==1 && last_value==0);
    reset(PAD_LENS_NON_POWER_ZOOM); capabilities.zoom_known=capabilities.zoom_enabled=true;
    capabilities.focus_known=capabilities.manual_focus=true;
    assert(submit(PAD_ACTION_ZOOM,1) && tick()==CAMERA_BACKEND_OK && !writes);
    reset(PAD_LENS_POWER_ZOOM); cancel_during_read=true;
    assert(submit(PAD_ACTION_ZOOM,1) && tick()==CAMERA_BACKEND_OK && !writes);
    cancel_during_read=false;
    assert(tick()==CAMERA_BACKEND_OK && writes==1 && last_value==0);
    reset(PAD_LENS_POWER_ZOOM); action_result=CAMERA_BACKEND_REFUSED;
    assert(submit(PAD_ACTION_S2,1) && tick()==CAMERA_BACKEND_OK && controls.actions.s2);
    assert(tick()==CAMERA_BACKEND_REFUSED && controls.actions.s2);
    action_result=CAMERA_BACKEND_OK;
    assert(tick()==CAMERA_BACKEND_OK && !controls.actions.s2);
    reset(PAD_LENS_POWER_ZOOM); read_result=CAMERA_BACKEND_NETWORK;
    assert(submit(PAD_ACTION_ZOOM,1) && tick()==CAMERA_BACKEND_NETWORK && !writes && controls.actions.zoom);
    reset(PAD_LENS_POWER_ZOOM); read_delay=500;
    assert(submit(PAD_ACTION_RECORD,1) && tick()==CAMERA_BACKEND_TIMEOUT && !writes);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_S1,1)); clock_ms=1000;
    assert(tick()==CAMERA_BACKEND_OK && !writes); /* Expired press never starts I/O. */
    assert(!camera_controls_submit(&controls,(pad_action_t){PAD_ACTION_S1,1,controls.actions.generation-1},clock_ms));
    /* Unsigned clock wrap still confirms/expirs at the same ten-second bound. */
    clock_ms=UINT32_MAX-20;
    assert(submit(PAD_ACTION_RECORD,1) && tick()==CAMERA_BACKEND_OK);
    camera_controls_observe(&controls,&capabilities,controls.record_deadline);
    assert(controls.status[APP_CAMERA_CONTROL_RECORD]==SETTING_TIMEOUT);
    camera_controls_session(&controls,false,PAD_LENS_POWER_ZOOM);
    assert(!controls.caps.session && !controls.record_wait && !submit(PAD_ACTION_S1,1));
    assert(!guard_depth && enters==leaves && enters>30);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_S1,1) && direct()==CAMERA_BACKEND_OK && writes==1 && !reads);
    assert(submit(PAD_ACTION_RECORD,1) && direct()==CAMERA_BACKEND_OK && writes==1 && !reads);
    uint32_t old_generation=controls.actions.generation;
    assert(submit(PAD_ACTION_S1,0));
    assert(direct()==CAMERA_BACKEND_OK && writes==2 && !reads && last_value==0);
    assert(!controls.actions.s1 && !controls.actions.count && controls.actions.generation!=old_generation);
    assert(!controls.caps.record_pending);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_ZOOM,1) && direct()==CAMERA_BACKEND_OK && !writes && !reads);
    assert(tick()==CAMERA_BACKEND_OK && writes==1 && reads==1 && controls.actions.zoom==1);
    assert(submit(PAD_ACTION_ZOOM,0) && direct()==CAMERA_BACKEND_OK && writes==2 && reads==1);
    assert(!controls.actions.zoom && last_operation==CAMERA_ACTION_ZOOM && !last_value);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_S1,1) && direct()==CAMERA_BACKEND_OK);
    assert(submit(PAD_ACTION_RELEASE_ALL,0) && direct()==CAMERA_BACKEND_OK);
    assert(controls.actions.release_pending && !controls.actions.s1);
    assert(submit(PAD_ACTION_RECORD,1) && direct()==CAMERA_BACKEND_OK && writes==2 && !reads);
    assert(controls.actions.count==1 && controls.caps.record_pending);
    reset(PAD_LENS_POWER_ZOOM);
    assert(submit(PAD_ACTION_RECORD,1)); clock_ms=1000;
    assert(direct()==CAMERA_BACKEND_OK && !writes && !reads && !controls.actions.count && !controls.caps.record_pending);
    controls.leave=NULL;
    unsigned before_reads=reads, before_writes=writes;
    assert(tick()==CAMERA_BACKEND_INVALID && reads==before_reads && writes==before_writes);
    assert(!guard_depth && enters==leaves);
    return 0;
}
