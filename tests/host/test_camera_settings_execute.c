#include "camera_settings_execute.h"
#include <assert.h>
#include <string.h>
static camera_value_t mode_choices[]={{CAMERA_VALUE_U32,1},{CAMERA_VALUE_U32,2}};
static camera_value_t ev_choices[]={{CAMERA_VALUE_I16,0xff00},{CAMERA_VALUE_I16,0xffff},{CAMERA_VALUE_I16,0}};
static camera_value_t focus_choices[]={{CAMERA_VALUE_U16,2},{CAMERA_VALUE_U16,1},{CAMERA_VALUE_U16,4}};
static camera_property_t descriptors[4];
static camera_properties_t properties;
static setting_control_t mode;
static camera_menu_t menu;
static unsigned writes,steps;
static camera_setting_t last_setting;
static camera_value_t last_value;
static int last_direction;
static uint32_t clock_ms;
static bool fresh,issued;
static camera_backend_result_t write_result;
static camera_backend_result_t read(void *context,void *scratch,size_t capacity,uint32_t timeout,
    camera_property_visitor_t visitor,void *visitor_context,camera_capabilities_t *caps)
{
    (void)context; assert(scratch && capacity==16 && timeout==5000);
    *caps=(camera_capabilities_t){0};
    for (unsigned i=0;i<4;++i) visitor(visitor_context,&descriptors[i]);
    return CAMERA_BACKEND_OK;
}
static camera_backend_result_t set(void *context,camera_setting_t setting,camera_value_t value,uint32_t timeout)
{
    (void)context; assert(timeout==5000); ++writes; last_setting=setting; last_value=value;
    return write_result;
}
static camera_backend_result_t step(void *context,camera_setting_t setting,int direction,uint32_t timeout)
{
    (void)context; assert(timeout==5000 && (direction==-1 || direction==1));
    ++steps; last_setting=setting; last_direction=direction; return write_result;
}
static const camera_backend_ops_t ops={.properties=read,.set=set,.step=step};
static camera_backend_t backend={.api_version=CAMERA_BACKEND_API_VERSION,
    .capabilities=CAMERA_BACKEND_CAP_PROPERTIES,.ops=&ops};
static void refresh(void)
{
    unsigned char scratch[16];
    assert(camera_properties_read(&backend,scratch,sizeof scratch,5000,&properties)==CAMERA_BACKEND_OK);
    assert(camera_properties_apply(&properties,&mode,&menu,clock_ms)); fresh=true;
}
static void reset(void)
{
    mode=(setting_control_t){0}; menu=(camera_menu_t){0};
    writes=steps=clock_ms=0; write_result=CAMERA_BACKEND_OK; backend.ops=&ops;
    descriptors[0]=(camera_property_t){.setting=CAMERA_SETTING_MODE,.current={CAMERA_VALUE_U32,1},
        .writable=true,.choices=mode_choices,.choice_count=2};
    descriptors[1]=(camera_property_t){.setting=CAMERA_SETTING_EV,.current={CAMERA_VALUE_I16,0xffff},
        .writable=true,.choices=ev_choices,.choice_count=3};
    descriptors[2]=(camera_property_t){.setting=CAMERA_SETTING_FOCUS,.current={CAMERA_VALUE_U16,2},
        .writable=true,.choices=focus_choices,.choice_count=3};
    descriptors[3]=(camera_property_t){.setting=CAMERA_SETTING_APERTURE,.current={CAMERA_VALUE_U16,400},
        .writable=true,.relative=true};
    refresh();
}
static camera_backend_result_t execute(void)
{ return camera_settings_execute(&backend,&properties,&mode,&menu,&fresh,clock_ms,5000,&issued); }
int main(void)
{
    reset();
    assert(setting_control_step(&mode,1));
    assert(camera_menu_step(&menu,MENU_FOCUS,1,true));
    assert(camera_menu_step(&menu,MENU_EV,1,true));
    assert(execute()==CAMERA_BACKEND_OK && issued && writes==1 && last_setting==CAMERA_SETTING_MODE);
    assert(last_value.type==CAMERA_VALUE_U32 && last_value.bits==2 && mode.awaiting && !fresh);
    assert(mode.snapshot.current==1 && mode.status==SETTING_PENDING); /* Acceptance is not actual. */
    assert(execute()==CAMERA_BACKEND_OK && !issued && writes==1);
    clock_ms=500; refresh();
    assert(execute()==CAMERA_BACKEND_OK && !issued && writes==1 && !fresh); /* Mode barrier. */
    descriptors[0].current.bits=2; clock_ms=1000; refresh();
    assert(mode.status==SETTING_APPLIED);
    assert(execute()==CAMERA_BACKEND_OK && issued && writes==2 && last_setting==CAMERA_SETTING_EV);
    assert(last_value.type==CAMERA_VALUE_I16 && last_value.bits==0 && menu.items[MENU_EV].actual==0xffff);
    assert(execute()==CAMERA_BACKEND_OK && !issued && writes==2); /* One write per copied read. */
    descriptors[1].current.bits=0; clock_ms=1500; refresh();
    assert(execute()==CAMERA_BACKEND_OK && issued && writes==3 && last_setting==CAMERA_SETTING_FOCUS);
    assert(last_value.type==CAMERA_VALUE_U16 && last_value.bits==1);
    descriptors[2].current.bits=1; clock_ms=2000; refresh();
    assert(camera_menu_status(&menu,MENU_FOCUS)==SETTING_APPLIED);
    assert(camera_menu_step(&menu,MENU_APERTURE,-1,true));
    assert(execute()==CAMERA_BACKEND_OK && issued && steps==1 && last_direction==-1 && last_setting==CAMERA_SETTING_APERTURE);
    assert(menu.items[MENU_APERTURE].awaiting && menu.items[MENU_APERTURE].actual==400);
    descriptors[3].current.bits=280; clock_ms=2500; refresh();
    assert(camera_menu_status(&menu,MENU_APERTURE)==SETTING_APPLIED);
    reset(); write_result=CAMERA_BACKEND_REFUSED;
    assert(setting_control_step(&mode,1));
    assert(execute()==CAMERA_BACKEND_OK && issued && mode.status==SETTING_REJECTED && !mode.awaiting);
    reset(); write_result=CAMERA_BACKEND_NETWORK;
    assert(camera_menu_step(&menu,MENU_EV,-1,true));
    assert(execute()==CAMERA_BACKEND_NETWORK && issued && camera_menu_status(&menu,MENU_EV)==SETTING_REJECTED);
    assert(last_value.type==CAMERA_VALUE_I16 && last_value.bits==0xff00); /* Signed bits survive. */
    reset(); assert(camera_menu_step(&menu,MENU_FOCUS,1,true));
    menu.items[MENU_FOCUS].type=99;
    assert(execute()==CAMERA_BACKEND_PROTOCOL && !issued && !writes);
    assert(camera_menu_status(&menu,MENU_FOCUS)==SETTING_REJECTED);
    reset(); assert(setting_control_step(&mode,1)); properties.seen[CAMERA_SETTING_MODE]=false;
    assert(execute()==CAMERA_BACKEND_PROTOCOL && !issued && !writes && mode.status==SETTING_REJECTED);
    reset(); assert(camera_menu_step(&menu,MENU_APERTURE,1,true));
    camera_backend_ops_t incomplete=ops; incomplete.step=NULL; backend.ops=&incomplete;
    assert(execute()==CAMERA_BACKEND_UNSUPPORTED && !issued && !steps);
    assert(camera_menu_status(&menu,MENU_APERTURE)==SETTING_REJECTED);
    reset(); assert(setting_control_step(&mode,1)); assert(camera_menu_step(&menu,MENU_FOCUS,1,true));
    properties.valid=false;
    assert(execute()==CAMERA_BACKEND_STATE && !issued && !writes && !mode.snapshot.writable && !mode.dirty);
    assert(!menu.items[MENU_FOCUS].writable && !menu.items[MENU_FOCUS].control.dirty);
    reset(); assert(setting_control_step(&mode,1)); assert(execute()==CAMERA_BACKEND_OK && issued);
    clock_ms=10000; refresh(); assert(mode.status==SETTING_TIMEOUT && !mode.awaiting);
    assert(execute()==CAMERA_BACKEND_OK && !issued && writes==1);
    return 0;
}
