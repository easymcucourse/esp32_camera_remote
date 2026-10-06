#include "camera_stream.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static camera_stream_t stream;
static uint8_t buffers[2][256];
static app_message_t held[2];
static unsigned held_count,reads,actions,sets,views,live_calls;
static uint32_t clock_ms;
static uint32_t action_elapsed,last_timeout;
static camera_backend_result_t live_result=CAMERA_BACKEND_OK,action_result=CAMERA_BACKEND_OK;
static camera_action_t last_action;
static int last_action_value;
static camera_setting_t last_setting;
static camera_value_t last_value;
static bool manual;
static bool cancel_during_read;
static camera_value_t mode_choices[]={{CAMERA_VALUE_U32,1},{CAMERA_VALUE_U32,2}};
static camera_value_t focus_choices[]={{CAMERA_VALUE_U16,2},{CAMERA_VALUE_U16,1}};
static camera_property_t descriptors[2];
void *test_output_calloc(size_t n,size_t size) { return calloc(n,size); }
void test_output_free(void *pointer) { free(pointer); }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->source==APP_ENDPOINT_CAMERA && message->generation==stream.generation);
    if (message->type==APP_MESSAGE_CAMERA_FRAME) {
        assert(held_count<2); held[held_count++]=*message; message->lease=NULL;
    } else {
        if (message->type==APP_MESSAGE_CAMERA_PROPERTIES) ++views;
        else assert(message->type==APP_MESSAGE_CAMERA_COMMAND_STATUS);
        app_message_release(message);
    }
    return ESP_OK;
}
static uint32_t now(void *context) { (void)context; return clock_ms; }
static camera_backend_result_t properties(void *context,void *scratch,size_t capacity,uint32_t timeout,
    camera_property_visitor_t visitor,void *visitor_context,camera_capabilities_t *caps)
{
    (void)context; assert(scratch && capacity==256 && timeout==5000); ++reads;
    if (cancel_during_read) { camera_stream_focus_cancel(&stream); cancel_during_read=false; }
    *caps=(camera_capabilities_t){.focus_known=true,.manual_focus=manual,.recording_known=true};
    visitor(visitor_context,&descriptors[0]); visitor(visitor_context,&descriptors[1]);
    return CAMERA_BACKEND_OK;
}
static camera_backend_result_t action(void *context,camera_action_t operation,int value,uint32_t timeout)
{ (void)context; assert(timeout>0 && timeout<=5000); ++actions; last_timeout=timeout; clock_ms+=action_elapsed; last_action=operation; last_action_value=value; return action_result; }
static camera_backend_result_t set(void *context,camera_setting_t setting,camera_value_t value,uint32_t timeout)
{ (void)context; assert(timeout==5000); ++sets; last_setting=setting; last_value=value; return CAMERA_BACKEND_OK; }
static camera_backend_result_t events(void *context,uint32_t timeout,bool *changed)
{ (void)context; assert(timeout==5000); *changed=false; return CAMERA_BACKEND_OK; }
static camera_backend_result_t liveview(void *context,void *scratch,size_t capacity,uint32_t timeout,camera_frame_t *frame)
{
    (void)context; assert(scratch && capacity==256 && timeout==5000); ++live_calls;
    if (live_result==CAMERA_BACKEND_OK) *frame=(camera_frame_t){scratch,8};
    return live_result;
}
static const camera_backend_ops_t ops={.api_version=CAMERA_BACKEND_API_VERSION,
    .properties=properties,.action=action,.set=set,.events=events,.liveview=liveview};
static camera_backend_t backend={.api_version=CAMERA_BACKEND_API_VERSION,
    .capabilities=CAMERA_BACKEND_CAP_PROPERTIES|CAMERA_BACKEND_CAP_ACTIONS|CAMERA_BACKEND_CAP_LIVEVIEW|CAMERA_BACKEND_CAP_EVENTS,.ops=&ops};
static camera_backend_result_t tick(void) { return camera_stream_tick(&stream,&backend,5000,now,NULL); }
static void complete(void)
{
    for (unsigned i=0;i<held_count;++i) {
        app_message_t result={.source=APP_ENDPOINT_UI,.type=APP_MESSAGE_UI_FRAME_RESULT,
            .generation=held[i].generation,.result=ESP_OK};
        result.payload.command.token=held[i].payload.command.token;
        assert(camera_stream_message(&stream,&result,clock_ms)==ESP_OK);
        app_message_release(&held[i]);
    }
    held_count=0;
}
static void begin(pad_lens_t lens)
{
    assert(!held_count); stream=(camera_stream_t){0};
    clock_ms=reads=actions=sets=views=live_calls=action_elapsed=last_timeout=0; live_result=action_result=CAMERA_BACKEND_OK; manual=false;
    descriptors[0]=(camera_property_t){.setting=CAMERA_SETTING_MODE,.current={CAMERA_VALUE_U32,1},
        .writable=true,.choices=mode_choices,.choice_count=2};
    descriptors[1]=(camera_property_t){.setting=CAMERA_SETTING_FOCUS,.current={CAMERA_VALUE_U16,2},
        .writable=true,.choices=focus_choices,.choice_count=2};
    assert(camera_stream_begin(&stream,9,buffers[0],buffers[1],256,lens,clock_ms));
}
static esp_err_t submit(pad_action_type_t type,int value)
{
    app_message_t message={.type=APP_MESSAGE_CAMERA_ACTION};
    message.payload.action=(pad_action_t){type,value,stream.controls.caps.generation};
    return camera_stream_message(&stream,&message,clock_ms);
}
int main(void)
{
    begin(PAD_LENS_POWER_ZOOM);
    assert(tick()==CAMERA_BACKEND_OK && reads==1 && held_count==1 && views);
    assert(tick()==CAMERA_BACKEND_OK && held_count==2 && reads==1);
    assert(submit(PAD_ACTION_S1,1)==ESP_OK && tick()==CAMERA_BACKEND_OK && actions==1 && reads==1);
    assert(submit(PAD_ACTION_S2,1)==ESP_OK && tick()==CAMERA_BACKEND_OK && actions==2 && held_count==2);
    assert(submit(PAD_ACTION_RECORD,1)==ESP_OK && tick()==CAMERA_BACKEND_OK && actions==2 && reads==1);
    camera_stream_stop(&stream,clock_ms);
    assert(!camera_stream_end(&stream));
    assert(camera_stream_release(&stream,&backend,5000,now,NULL)==CAMERA_BACKEND_OK && actions==4 && !last_action_value);
    assert(!stream.controls.actions.s1 && !stream.controls.actions.s2 && !stream.controls.actions.count);
    assert(!camera_frames_drained(&stream.frames)); complete();
    assert(camera_stream_end(&stream) && !stream.controls.caps.session && !stream.mode.snapshot.writable);
    uint32_t previous=stream.controls.caps.generation;
    assert(camera_stream_begin(&stream,10,buffers[0],buffers[1],256,PAD_LENS_POWER_ZOOM,clock_ms));
    assert(stream.controls.caps.generation!=previous);
    camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); assert(tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_S1,1)==ESP_OK && tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_S2,1)==ESP_OK && tick()==CAMERA_BACKEND_OK); complete();
    camera_stream_stop(&stream,clock_ms); action_elapsed=300;
    assert(camera_stream_release(&stream,&backend,900,now,NULL)==CAMERA_BACKEND_OK);
    assert(actions==4 && last_timeout==600 && !stream.controls.actions.s1 && !stream.controls.actions.s2);
    assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); assert(tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_S1,1)==ESP_OK && tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_S2,1)==ESP_OK && tick()==CAMERA_BACKEND_OK); complete();
    camera_stream_stop(&stream,clock_ms); action_elapsed=900;
    assert(camera_stream_release(&stream,&backend,900,now,NULL)==CAMERA_BACKEND_TIMEOUT);
    assert(actions==3 && stream.controls.actions.s1 && !stream.controls.actions.s2);
    action_elapsed=0;
    assert(camera_stream_release(&stream,&backend,900,now,NULL)==CAMERA_BACKEND_OK && actions==4);
    assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); assert(tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_MODE_NEXT,1)==ESP_OK && submit(PAD_ACTION_FOCUS_MODE_NEXT,1)==ESP_OK);
    assert(tick()==CAMERA_BACKEND_OK && sets==1 && last_setting==CAMERA_SETTING_MODE && last_value.bits==2);
    assert(stream.mode.awaiting && stream.mode.snapshot.current==1); complete();
    clock_ms=500; assert(tick()==CAMERA_BACKEND_OK && sets==1); complete();
    descriptors[0].current.bits=2; clock_ms=1000;
    assert(tick()==CAMERA_BACKEND_OK && sets==2 && last_setting==CAMERA_SETTING_FOCUS && last_value.bits==1);
    complete(); camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    begin(PAD_LENS_NON_POWER_ZOOM); manual=true;
    assert(tick()==CAMERA_BACKEND_OK); complete();
    assert(submit(PAD_ACTION_MF_STEP,1)==ESP_OK && tick()==CAMERA_BACKEND_OK);
    assert(last_action==CAMERA_ACTION_FOCUS_STEP && last_action_value==1 && reads==2); complete();
    cancel_during_read=true;
    assert(submit(PAD_ACTION_MF_STEP,1)==ESP_OK && tick()==CAMERA_BACKEND_OK && actions==1); complete();
    assert(submit(PAD_ACTION_MF_STEP,-1)==ESP_OK && submit(PAD_ACTION_MF_CANCEL,0)==ESP_OK);
    assert(tick()==CAMERA_BACKEND_OK && actions==1); complete();
    assert(submit(PAD_ACTION_MF_STEP,-1)==ESP_OK); manual=false;
    assert(tick()==CAMERA_BACKEND_OK && actions==1); complete();
    camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); live_result=CAMERA_BACKEND_NOT_READY;
    for (unsigned i=0;i<50;++i) { assert(tick()==CAMERA_BACKEND_OK); clock_ms+=100; }
    assert(tick()==CAMERA_BACKEND_NOT_READY && live_calls==51 && !held_count);
    camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); live_result=CAMERA_BACKEND_REFUSED;
    assert(tick()==CAMERA_BACKEND_REFUSED && live_calls==1);
    camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    begin(PAD_LENS_POWER_ZOOM); live_result=CAMERA_BACKEND_DROPPED;
    for (unsigned i=0;i<10;++i) assert(tick()==CAMERA_BACKEND_OK);
    assert(stream.frames.failed && stream.frames.bad_streak==10 && !held_count && tick()==CAMERA_BACKEND_STATE);
    camera_stream_stop(&stream,clock_ms); assert(camera_stream_end(&stream));
    return 0;
}
