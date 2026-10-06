#include "input_owner.h"
#include <assert.h>
#include <stdio.h>

typedef struct { pad_action_t actions[128]; unsigned count; } trace_t;
static int64_t clock_us;
int64_t esp_timer_get_time(void) { return clock_us; }
static bool collect(void *context, pad_action_t action)
{
    trace_t *trace=context;
    assert(trace->count<128);
    trace->actions[trace->count++]=action;
    return true;
}
static void replay(input_source_kind_t source, trace_t *trace)
{
    input_owner_t owner;
    input_provider_handle_t handle;
    gamepad_caps_t caps={.session=true,.settings=true,.generation=17,
        .lens=PAD_LENS_POWER_ZOOM,.recording_known=true};
    clock_us=0;
    assert(input_provider_registry_init()==ESP_OK);
    assert(input_provider_register(source,&handle)==ESP_OK);
    input_owner_init(&owner,collect,trace);
    assert(input_owner_select(&owner,source));
    input_report_t report={.connected=true,.atom_online=source==INPUT_SOURCE_ATOM,
        .sim=source==INPUT_SOURCE_UART_SIM,.source_epoch=1,.battery=8};
    /* The first report is a baseline. Drop source-selection housekeeping,
     * then compare every action from the same normalized report timeline. */
    report.report_id=1;
    assert(input_provider_publish(handle,&report)==ESP_OK);
    input_owner_tick(&owner,&caps,0);
    trace->count=0;
    const struct { unsigned ms; uint32_t buttons; uint8_t lt,rt; bool gap; } steps[]={
        {50,0,0,77,false},{100,0,0,230,false},{150,0,0,203,false},
        {200,0,0,50,false},{250,0,230,0,false},{300,0,203,0,false},
        {350,PAD_L1,0,0,false},{400,0,0,0,false},
        {450,PAD_Y,0,0,false},{500,0,0,0,false},
        {550,PAD_X,0,0,false},{600,0,0,0,false},
        {650,PAD_DOWN,0,0,false},{1050,PAD_DOWN,0,0,false},
        {1100,0,0,0,false},{1150,PAD_RIGHT,0,0,false},{1200,0,0,0,false},
        {1250,PAD_A,0,0,false},{1300,0,0,0,false},
        {1350,PAD_B,0,0,false},{1400,0,0,0,false},
        {1450,PAD_START,0,0,false},{1500,0,0,0,false},
        {1550,0,0,255,false},{1600,PAD_Y,0,255,true},
        {1650,0,0,0,false},{1700,0,0,255,false}
    };
    for (unsigned i=0;i<sizeof(steps)/sizeof(steps[0]);++i) {
        unsigned before=trace->count;
        report.report_id++;
        report.buttons=report.event_buttons=steps[i].buttons;
        report.event_valid=true;report.lt=steps[i].lt;report.rt=steps[i].rt;
        report.gap=steps[i].gap;clock_us=(int64_t)steps[i].ms*1000;
        assert(input_provider_publish(handle,&report)==ESP_OK);
        input_owner_tick(&owner,&caps,steps[i].ms);
        if(steps[i].gap){
            assert(trace->count>=before+2);
            assert(trace->actions[before].type==PAD_ACTION_RELEASE_ALL && !trace->actions[before].value);
            assert(trace->actions[before+1].type==PAD_ACTION_MF_CANCEL && !trace->actions[before+1].value);
            for(unsigned j=before;j<trace->count;++j)
                assert(trace->actions[j].type==PAD_ACTION_RELEASE_ALL || trace->actions[j].type==PAD_ACTION_MF_CANCEL);
        }
    }
    unsigned before=trace->count;
    assert(input_provider_disconnect(handle,INPUT_DISCONNECT_OFFLINE)==ESP_OK);
    input_owner_tick(&owner,&caps,1750);
    assert(!owner.latest.connected && !owner.reports.gamepad.connected);
    assert(trace->count>=before+2 && trace->actions[before].type==PAD_ACTION_RELEASE_ALL &&
        trace->actions[before+1].type==PAD_ACTION_MF_CANCEL);
    assert(input_owner_quiesce(&owner));
    assert(input_provider_unregister(handle)==ESP_OK);
    input_owner_tick(&owner,&caps,1800);
    assert(input_provider_registry_deinit()==ESP_OK);
}
int main(void)
{
    trace_t atom={0},sim={0};replay(INPUT_SOURCE_ATOM,&atom);replay(INPUT_SOURCE_UART_SIM,&sim);
    assert(atom.count==sim.count && atom.count>20);
    bool seen[PAD_ACTION_MAINT_TOGGLE+1]={0};
    for (unsigned i=0;i<atom.count;++i) {
        assert(atom.actions[i].type==sim.actions[i].type);
        assert(atom.actions[i].value==sim.actions[i].value);
        assert(atom.actions[i].generation==sim.actions[i].generation);
        seen[atom.actions[i].type]=true;
    }
    assert(seen[PAD_ACTION_S1] && seen[PAD_ACTION_S2] && seen[PAD_ACTION_RECORD]);
    assert(seen[PAD_ACTION_ZOOM] && seen[PAD_ACTION_MODE_NEXT] && seen[PAD_ACTION_FOCUS_MODE_NEXT]);
    assert(seen[PAD_ACTION_MENU_MOVE] && seen[PAD_ACTION_MENU_STEP]);
    assert(seen[PAD_ACTION_MENU_CONFIRM] && seen[PAD_ACTION_MENU_BACK] && seen[PAD_ACTION_UI_TOGGLE]);
    assert(seen[PAD_ACTION_RELEASE_ALL] && seen[PAD_ACTION_MF_CANCEL]);
    printf("ATOM/SIM normalized reports produce %u identical actions including gap/offline release\n",atom.count);
    return 0;
}
