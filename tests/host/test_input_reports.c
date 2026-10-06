#include "input_reports.h"
#include <assert.h>
#include <stdio.h>

static unsigned presses, releases;
static bool reject_release, reject_mf;
static bool sink(void *context, pad_action_t a)
{
    (void)context;
    if (a.type == PAD_ACTION_RELEASE_ALL) { ++releases; return !reject_release; }
    if (a.type == PAD_ACTION_MF_CANCEL) return !reject_mf;
    if ((a.type == PAD_ACTION_S1 || a.type == PAD_ACTION_S2) && a.value) ++presses;
    return true;
}
static input_reports_t state;
static gamepad_caps_t caps = {.session=true,.generation=7};
static input_report_frame_t report;
static void begin(void)
{
    presses=releases=0; reject_release=reject_mf=false;
    input_reports_init(&state,sink,NULL);
    assert(input_reports_select(&state,INPUT_REPORT_ATOM));
    report=(input_report_frame_t){.snapshot={.connected=true,.battery=100},.source_epoch=1,.report_id=1};
    assert(input_reports_publish(&state,INPUT_REPORT_ATOM,&report,&caps,0));
}
static bool publish(void)
{ return input_reports_publish(&state,INPUT_REPORT_ATOM,&report,&caps,10); }
int main(void)
{
    begin(); report.report_id=2;report.snapshot.rt=255;assert(publish());assert(presses==2);
    unsigned old=releases;assert(!publish());assert(releases==old && presses==2);
    report.report_id=1;assert(!publish());assert(releases>old && !state.gamepad.connected);
    report.report_id=3;assert(!publish()); /* Epoch remains quarantined. */
    report.source_epoch=2;report.report_id=1;assert(publish());assert(presses==2);
    report.report_id=2;report.snapshot.rt=0;assert(publish());
    report.report_id=3;report.snapshot.rt=255;assert(publish());assert(presses==4);
    old=releases;report.source_epoch=1;report.report_id=100;assert(!publish());assert(releases==old);

    begin();report.report_id=2;report.snapshot.rt=255;assert(publish());
    reject_release=true;assert(!input_reports_select(&state,INPUT_REPORT_SIM));
    input_report_frame_t sim={.snapshot={.connected=true,.rt=255,.battery=255},.source_epoch=1,.report_id=1};
    assert(!input_reports_publish(&state,INPUT_REPORT_SIM,&sim,&caps,20));assert(presses==2);
    assert(!publish()); /* An old source never reclaims selection. */
    reject_release=false;reject_mf=true;
    input_reports_disconnect(&state,INPUT_REPORT_SIM);
    assert(!input_reports_publish(&state,INPUT_REPORT_SIM,&sim,&caps,20));
    reject_mf=false;assert(input_reports_publish(&state,INPUT_REPORT_SIM,&sim,&caps,20));assert(presses==2);
    sim.report_id=2;sim.snapshot.rt=0;assert(input_reports_publish(&state,INPUT_REPORT_SIM,&sim,&caps,30));
    sim.report_id=3;sim.snapshot.rt=255;assert(input_reports_publish(&state,INPUT_REPORT_SIM,&sim,&caps,40));assert(presses==4);
    input_reports_disconnect(&state,INPUT_REPORT_SIM);assert(!state.gamepad.connected);

    begin();report.report_id=2;report.snapshot.rt=255;assert(publish());
    report.report_id=3;report.gap=true;old=releases;assert(publish());assert(releases>old && presses==2);
    report.report_id=4;report.gap=false;assert(publish());assert(presses==2);
    report.report_id=5;report.snapshot.rt=0;assert(publish());
    report.report_id=6;report.snapshot.rt=255;assert(publish());assert(presses==4);
    report.report_id=7;report.snapshot.connected=false;assert(publish());assert(!state.gamepad.connected);
    assert(input_reports_select(&state,INPUT_REPORT_NONE));assert(!publish());
    assert(!input_reports_select(&state,INPUT_REPORT_SOURCE_COUNT));
    puts("input report source, epoch, gap, duplicate and release barriers passed");
}
