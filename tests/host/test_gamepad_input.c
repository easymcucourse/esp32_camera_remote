#include <assert.h>
#include <stdio.h>
#include "gamepad_input.h"

static pad_action_t actions[256];
static unsigned count;
static bool reject_next;
static bool observe(void *context, pad_action_t a)
{
    (void)context;
    if (a.type == PAD_ACTION_MF_CANCEL) return true;
    assert(count < 256); actions[count++] = a;
    if (reject_next && a.type != PAD_ACTION_RELEASE_ALL) { reject_next = false; return false; }
    return true;
}
static void expect(unsigned i, pad_action_type_t type, int value)
{
    assert(i < count && actions[i].type == type && actions[i].value == value);
}
static gamepad_input_t s;
static gamepad_caps_t caps;
static gamepad_snapshot_t pad;
static void start(void)
{
    gamepad_input_init(&s, observe, NULL);
    caps = (gamepad_caps_t){.session = true, .mf_known = true, .zoom_known = true,
        .zoom_enabled = true, .recording_known = true, .lens = PAD_LENS_POWER_ZOOM, .generation = 1};
    pad = (gamepad_snapshot_t){.connected = true};
    gamepad_input_online(&s, &pad, &caps);
    gamepad_input_snapshot(&s, &pad, &caps, 0); count = 0;
}
static void snapshot(uint8_t lt, uint8_t rt, uint32_t now)
{
    pad.lt = lt; pad.rt = rt; gamepad_input_snapshot(&s, &pad, &caps, now);
}
static void event(uint32_t buttons, uint32_t now)
{
    pad.buttons = buttons; gamepad_input_event(&s, buttons, false, &caps, now);
}
int main(void)
{
    start();
    /* Both face keys use one-shot edges and never act as MF steps. */
    event(PAD_X | PAD_Y | PAD_START, 10);
    assert(count == 3); expect(0, PAD_ACTION_UI_TOGGLE, 1);
    expect(1, PAD_ACTION_MODE_NEXT, 1); expect(2, PAD_ACTION_FOCUS_MODE_NEXT, 1);
    event(pad.buttons, 100); snapshot(0, 0, 1000); assert(count == 3);
    event(0, 1100); event(PAD_Y, 1110); assert(count == 4);

    start(); snapshot(0, 76, 1); assert(count == 0);
    snapshot(0, 77, 2); expect(0, PAD_ACTION_S1, 1);
    snapshot(0, 51, 3); snapshot(0, 229, 4); assert(count == 1);
    snapshot(0, 230, 5); expect(1, PAD_ACTION_S2, 1);
    snapshot(0, 204, 6); assert(count == 2);
    snapshot(0, 203, 7); expect(2, PAD_ACTION_S2, 0);
    snapshot(0, 50, 8); expect(3, PAD_ACTION_S1, 0);
    /* Direct off -> full and full -> off preserve S1/S2 order. */
    count = 0; snapshot(0, 255, 10); snapshot(0, 0, 11);
    assert(count == 4); expect(0, PAD_ACTION_S1, 1); expect(1, PAD_ACTION_S2, 1);
    expect(2, PAD_ACTION_S2, 0); expect(3, PAD_ACTION_S1, 0);

    start(); snapshot(0, 100, 1); snapshot(100, 100, 2);
    snapshot(100, 0, 3); assert(count == 1);
    snapshot(0, 0, 4); assert(count == 2); expect(1, PAD_ACTION_S1, 0);
    /* LT full emits a single explicit recording target. */
    start(); snapshot(255, 0, 1); snapshot(255, 0, 2);
    assert(count == 2); expect(0, PAD_ACTION_S1, 1); expect(1, PAD_ACTION_RECORD, 1);
    caps.record_pending = true; snapshot(100, 0, 3); snapshot(255, 0, 4);
    expect(2, PAD_ACTION_RECORD_UNAVAILABLE, 0);
    caps.record_pending = false; caps.recording = true;
    snapshot(100, 0, 5); snapshot(255, 0, 6); expect(3, PAD_ACTION_RECORD, 0);
    caps.recording_known = false; snapshot(100, 0, 7); snapshot(255, 0, 8);
    expect(4, PAD_ACTION_RECORD_UNAVAILABLE, 0);

    /* Bringing pressed triggers online or opening a session cannot shoot. */
    start(); gamepad_input_offline(&s); pad.lt = pad.rt = 255;
    gamepad_input_online(&s, &pad, &caps); count = 0;
    snapshot(255, 255, 1); snapshot(60, 60, 2); assert(count == 0);
    snapshot(0, 0, 3); snapshot(0, 255, 4); assert(count == 2);
    caps.generation++; snapshot(0, 255, 5); expect(2, PAD_ACTION_RELEASE_ALL, 0);
    snapshot(0, 255, 6); assert(count == 3);
    snapshot(0, 0, 7); snapshot(0, 255, 8); assert(count == 5);

    /* Zoom keeps running until release and locks on simultaneous shoulders. */
    start(); caps.mf = true; snapshot(0, 0, 1); event(PAD_L1, 2);
    expect(0, PAD_ACTION_ZOOM, -1); snapshot(0, 0, 900); assert(count == 1);
    event(PAD_SHOULDERS, 901); expect(1, PAD_ACTION_ZOOM, 0);
    event(PAD_R1, 902); snapshot(0, 0, 1500); assert(count == 2);
    event(0, 1501); event(PAD_R1, 1502); expect(2, PAD_ACTION_ZOOM, 1);
    event(0, 1503); expect(3, PAD_ACTION_ZOOM, 0);

    /* X stops a held shoulder and requires a fresh shoulder press. */
    start(); event(PAD_L1, 1); event(PAD_L1 | PAD_X, 2);
    expect(0, PAD_ACTION_ZOOM, -1); expect(1, PAD_ACTION_ZOOM, 0);
    expect(2, PAD_ACTION_FOCUS_MODE_NEXT, 1);
    snapshot(0, 0, 1000); event(PAD_L1, 1001); assert(count == 3);
    event(0, 1002); event(PAD_L1, 1003); expect(3, PAD_ACTION_ZOOM, -1);

    /* Non-power-zoom MF wins even when digital zoom is available. */
    start(); caps.lens = PAD_LENS_NON_POWER_ZOOM; caps.mf = true;
    snapshot(0, 0, 0); event(PAD_L1, 10); expect(0, PAD_ACTION_MF_STEP, 1);
    snapshot(0, 0, 409); assert(count == 1);
    snapshot(0, 0, 410); expect(1, PAD_ACTION_MF_STEP, 1);
    snapshot(0, 0, 10000); expect(2, PAD_ACTION_MF_STEP, 1);
    snapshot(0, 0, 10001); assert(count == 3);
    event(0, 10002); event(PAD_R1, 10003); expect(3, PAD_ACTION_MF_STEP, -1);
    caps.mf = false; snapshot(0, 0, 10004); event(PAD_R1, 10005); assert(count == 4);
    event(0, 10006); event(PAD_R1, 10007); expect(4, PAD_ACTION_ZOOM, 1);
    caps.zoom_enabled = false; snapshot(0, 0, 10008); expect(5, PAD_ACTION_ZOOM, 0);
    event(0, 10009); event(PAD_L1, 10010); assert(count == 6);

    /* Unknown lens/AF/zoom never accidentally enables manual focus. */
    start(); caps.lens = PAD_LENS_UNKNOWN; caps.mf = true; caps.zoom_known = false;
    snapshot(0, 0, 0); event(PAD_L1, 1); assert(count == 0);
    caps.lens = PAD_LENS_NON_POWER_ZOOM; caps.mf_known = false;
    event(0, 2); event(PAD_R1, 3); assert(count == 0);

    /* Confirmed PZ still uses zoom when the separate enable status is absent
     * or disabled; it must never become MF on this lens. Releases remain zero. */
    start();caps.zoom_known=false;caps.zoom_enabled=false;caps.mf=true;
    snapshot(0,0,0);event(PAD_L1,1);expect(0,PAD_ACTION_ZOOM,-1);
    event(0,2);expect(1,PAD_ACTION_ZOOM,0);
    caps.zoom_known=true;snapshot(0,0,3);event(PAD_R1,4);expect(2,PAD_ACTION_ZOOM,1);
    gamepad_input_offline(&s);expect(3,PAD_ACTION_RELEASE_ALL,0);

    /* Gap discards all pending effects, synchronizes held keys without edges. */
    start(); pad.buttons = PAD_X | PAD_Y | PAD_L1;
    gamepad_input_event(&s, pad.buttons, true, &caps, 1);
    expect(0, PAD_ACTION_RELEASE_ALL, 0); snapshot(0, 0, 2);
    event(pad.buttons = PAD_X | PAD_Y | PAD_L1, 3); assert(count == 1);
    event(0, 4); event(PAD_Y, 5); expect(1, PAD_ACTION_MODE_NEXT, 1);
    reject_next = true; snapshot(0, 255, 6); expect(2, PAD_ACTION_S1, 1);
    expect(3, PAD_ACTION_RELEASE_ALL, 0); assert(!s.trigger_armed && !s.s1 && !s.s2);
    gamepad_input_offline(&s); expect(4, PAD_ACTION_RELEASE_ALL, 0);
    /* Menu only in SETTINGS, 400ms then 150ms repeat, no replay/catch-up. */
    start(); event(PAD_RIGHT, 1); snapshot(0, 0, 1000); assert(count == 0);
    caps.settings = true; snapshot(0, 0, 1001); assert(count == 0);
    event(0, 1002); event(PAD_RIGHT, 1003); expect(0, PAD_ACTION_MENU_STEP, 1);
    snapshot(0, 0, 1402); assert(count == 1);
    snapshot(0, 0, 1403); expect(1, PAD_ACTION_MENU_STEP, 1);
    snapshot(0, 0, 9000); expect(2, PAD_ACTION_MENU_STEP, 1);
    snapshot(0, 0, 9001); assert(count == 3);
    event(PAD_RIGHT | PAD_LEFT, 9002); event(PAD_LEFT, 9003); snapshot(0, 0, 9999); assert(count == 3);
    event(0, 10000); event(PAD_UP, 10001); expect(3, PAD_ACTION_MENU_MOVE, -1);
    event(0, 10002); event(PAD_DOWN, 10003); expect(4, PAD_ACTION_MENU_MOVE, 1);
    caps.settings = false; snapshot(0, 0, 11000); assert(count == 5);
    caps.settings = true; snapshot(0, 0, 11001); assert(count == 5);
    event(0, 11002); event(PAD_LEFT, 11003); expect(5, PAD_ACTION_MENU_STEP, -1);
    gamepad_input_event(&s, PAD_LEFT, true, &caps, 11004); expect(6, PAD_ACTION_RELEASE_ALL, 0);
    snapshot(0, 0, 12000); assert(count == 7);
    event(0, 12001); event(PAD_LEFT, 12002); expect(7, PAD_ACTION_MENU_STEP, -1);
    /* Repeat deadline across wrap. */
    start(); caps.settings = true; snapshot(0, 0, UINT32_MAX - 201);
    event(PAD_RIGHT, UINT32_MAX - 200); snapshot(0, 0, 198); assert(count == 1);
    snapshot(0, 0, 199); expect(1, PAD_ACTION_MENU_STEP, 1);
    /* Offline hotspot settings still accept navigation and A/B edges. */
    start(); caps.settings = true; caps.session = false; snapshot(0, 0, 0); count = 0;
    event(PAD_A, 1); expect(0, PAD_ACTION_MENU_CONFIRM, 1);
    event(PAD_A, 2); assert(count == 1);
    event(0, 3); event(PAD_B, 4); expect(1, PAD_ACTION_MENU_BACK, 1);
    event(0, 5); event(PAD_DOWN, 6); expect(2, PAD_ACTION_MENU_MOVE, 1);
    event(PAD_DOWN | PAD_B, 7); expect(3, PAD_ACTION_MENU_BACK, 1);
    snapshot(0, 0, 999); assert(count == 4); /* B cancels held-direction repeat. */
    event(PAD_DOWN | PAD_X | PAD_Y | PAD_L1, 1000); assert(count == 4);
    event(0, 1001); event(PAD_UP, 1002); expect(4, PAD_ACTION_MENU_MOVE, -1);
    snapshot(0, 0, 1402); expect(5, PAD_ACTION_MENU_MOVE, -1);
    puts("gamepad input mapping and safety tests passed");
    start();caps.session=false;snapshot(0,0,0);count=0;
    event(PAD_TOUCH,1);expect(0,PAD_ACTION_UI_INFO_NEXT,1);
    event(PAD_TOUCH,2);assert(count==1);
    snapshot(0,0,1000);assert(count==1); /* Held/snapshot input never repeats. */
    gamepad_input_event(&s,0,true,&caps,1001);count=0;
    gamepad_input_event(&s,PAD_TOUCH,true,&caps,1002);
    assert(count==1 && actions[0].type==PAD_ACTION_RELEASE_ALL); /* Gap cannot switch information. */
    start();event(PAD_X|PAD_Y,1);
    for (unsigned i=0;i<count;++i) assert(actions[i].type!=PAD_ACTION_UI_INFO_NEXT);
    start();caps.session=false;snapshot(0,0,0);count=0;
    event(PAD_SELECT,10);snapshot(0,0,2009);assert(count==0);
    snapshot(0,0,2010);expect(0,PAD_ACTION_MAINT_TOGGLE,1);
    snapshot(0,0,9000);assert(count==1); /* One action per physical hold. */
    event(0,9001);event(PAD_SELECT,9002);event(0,9100);snapshot(0,0,12000);assert(count==1);
    event(PAD_SELECT,12001);gamepad_input_event(&s,PAD_SELECT,true,&caps,12002);count=0;
    snapshot(0,0,15000);assert(count==0); /* Gap cancels and held snapshot cannot rearm. */
    event(0,15001);event(PAD_SELECT,15002);caps.settings=true;snapshot(0,0,18000);assert(count==0);
    start();event(PAD_SELECT,0);snapshot(0,0,3000);assert(count==0); /* Live session forbids. */
    start();caps.session=false;snapshot(0,0,0);count=0;
    event(PAD_SELECT,UINT32_MAX-1000);snapshot(0,0,998);assert(count==0);
    snapshot(0,0,999);expect(0,PAD_ACTION_MAINT_TOGGLE,1);
    start();caps.session=false;pad.buttons=PAD_SELECT;
    gamepad_input_online(&s,&pad,&caps);count=0;snapshot(0,0,5000);assert(count==0);
    return 0;
}
