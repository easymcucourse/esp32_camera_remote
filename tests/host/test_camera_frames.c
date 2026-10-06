#include "camera_frames.h"
#include "app_message_internal.h"
#include <assert.h>
static app_message_t held[2];
static unsigned held_count;
static esp_err_t send_error;
static bool partial_delivery;
static void ignored_return(void *context) { (void)context; }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->source==APP_ENDPOINT_CAMERA && message->type==APP_MESSAGE_CAMERA_FRAME);
    assert(message->flags==(APP_MESSAGE_EVENT|APP_MESSAGE_BULK));
    if (send_error && !partial_delivery) { app_message_release(message); return send_error; }
    assert(held_count<2); held[held_count++]=*message; message->lease=NULL;
    return send_error;
}
static void result(camera_frames_t *f, unsigned index, esp_err_t error)
{
    app_message_t message={.type=APP_MESSAGE_UI_FRAME_RESULT,.source=APP_ENDPOINT_UI,
        .generation=f->generation,.result=error};
    message.payload.command.token=held[index].payload.command.token;
    assert(camera_frames_result(f,&message));
}
int main(void)
{
    uint8_t buffers[2][32]; camera_frames_t frames;
    assert(camera_frames_init(&frames,7,buffers[0],buffers[1],32));
    int first=camera_frames_acquire(&frames),second=camera_frames_acquire(&frames);
    assert(first==0 && second==1 && camera_frames_acquire(&frames)==-1);
    assert(camera_frames_publish(&frames,0,buffers[1],5,0)==ESP_ERR_INVALID_ARG);
    assert(camera_frames_publish(&frames,0,buffers[0]+31,2,0)==ESP_ERR_INVALID_ARG);
    assert(camera_frames_publish(&frames,0,buffers[0]+4,8,0)==ESP_OK);
    assert(camera_frames_publish(&frames,1,buffers[1],9,0)==ESP_OK);
    assert(!camera_frames_drained(&frames) && !app_message_lease_write(held[0].lease,NULL));
    size_t size; assert(app_message_lease_data(held[0].lease,&size)==buffers[0]+4 && size==8);
    assert(app_message_lease_retain(held[0].lease));
    app_message_t extra=held[0];
    app_message_release(&held[0]);
    result(&frames,1,ESP_OK); /* Metadata can arrive out of order. */
    assert(!frames.shown && camera_frames_acquire(&frames)==-1);
    result(&frames,0,ESP_ERR_INVALID_RESPONSE);
    assert(frames.shown==1 && frames.dropped==1 && !frames.bad_streak);
    assert(camera_frames_acquire(&frames)==-1); /* Both last references still pin buffers. */
    app_message_release(&extra); assert(camera_frames_acquire(&frames)==0);
    camera_frames_discard(&frames,0);
    app_message_release(&held[1]); assert(camera_frames_drained(&frames));
    app_message_t stale={.type=APP_MESSAGE_UI_FRAME_RESULT,.source=APP_ENDPOINT_UI,.generation=6};
    assert(!camera_frames_result(&frames,&stale)); stale.generation=7;
    stale.payload.command.token=1; assert(!camera_frames_result(&frames,&stale));
    held_count=0; send_error=ESP_ERR_TIMEOUT; partial_delivery=true;
    assert(camera_frames_acquire(&frames)==0);
    assert(camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_ERR_TIMEOUT);
    assert(!camera_frames_drained(&frames) && frames.dropped==2);
    stale.payload.command.token=held[0].payload.command.token;
    assert(!camera_frames_result(&frames,&stale));
    app_message_release(&held[0]); held_count=0; send_error=ESP_OK; partial_delivery=false;
    assert(camera_frames_acquire(&frames)==0);
    assert(camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_OK);
    assert(held[0].payload.command.token!=stale.payload.command.token);
    assert(!camera_frames_result(&frames,&stale));
    result(&frames,0,ESP_ERR_NOT_FINISHED); app_message_release(&held[0]);
    assert(!frames.bad_streak && frames.dropped==3 && !frames.failed);
    for (unsigned i=0;i<10;++i) {
        held_count=0; assert(camera_frames_acquire(&frames)==0);
        assert(camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_OK);
        result(&frames,0,ESP_ERR_INVALID_RESPONSE); app_message_release(&held[0]);
    }
    assert(frames.failed && frames.bad_streak==10 && camera_frames_acquire(&frames)==-1);
    assert(camera_frames_drained(&frames));
    assert(camera_frames_init(&frames,8,buffers[0],buffers[1],32)); held_count=0;
    assert(camera_frames_acquire(&frames)==0 && camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_OK);
    result(&frames,0,ESP_FAIL); assert(frames.failed && !camera_frames_drained(&frames));
    app_message_release(&held[0]); assert(camera_frames_drained(&frames));
    assert(camera_frames_init(&frames,9,buffers[0],buffers[1],32)); held_count=0;
    assert(camera_frames_acquire(&frames)==0 && camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_OK);
    assert(camera_frames_acquire(&frames)==1 && camera_frames_drop(&frames,1));
    assert(!frames.dropped); /* Local envelope failure waits for prior UI frame result. */
    result(&frames,0,ESP_OK); app_message_release(&held[0]);
    assert(frames.shown==1 && frames.dropped==1 && frames.bad_streak==1);
    for (unsigned i=0;i<9;++i) {
        int slot=camera_frames_acquire(&frames); assert(slot>=0 && camera_frames_drop(&frames,slot));
    }
    assert(frames.failed && frames.bad_streak==10 && camera_frames_drained(&frames));
    assert(!camera_frames_init(&frames,10,buffers[0],buffers[0]+1,32));
    assert(camera_frames_init(&frames,10,buffers[0],buffers[1],32));
    app_message_t pinned[APP_MESSAGE_LEASE_CAPACITY]={0};
    for (unsigned i=0;i<APP_MESSAGE_LEASE_CAPACITY;++i)
        assert(app_message_lease_create(buffers[0],1,false,ignored_return,NULL,&pinned[i].lease)==ESP_OK);
    assert(camera_frames_acquire(&frames)==0);
    assert(camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_ERR_NO_MEM);
    assert(camera_frames_drained(&frames) && frames.slots[0].acquired && frames.next_token==1);
    for (unsigned i=0;i<APP_MESSAGE_LEASE_CAPACITY;++i) app_message_release(&pinned[i]);
    held_count=0; assert(camera_frames_publish(&frames,0,buffers[0],8,0)==ESP_OK);
    result(&frames,0,ESP_OK); app_message_release(&held[0]);
    frames.next_token=0;
    assert(camera_frames_acquire(&frames)==0 && !camera_frames_drop(&frames,0) && frames.failed);
    return 0;
}
