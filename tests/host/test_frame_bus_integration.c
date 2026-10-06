#include "fake_router_runtime.h"
#include "app_message_internal.h"
#include "camera_frames.h"
#include "ui_frames.h"
#include "app_ui_internal.h"

static const uint8_t *expected_jpeg;
static size_t expected_length;
static unsigned shown;
static uint32_t generation;
bool ui_camera_generation_accept(uint32_t next)
{ if (!next || (generation && (int32_t)(next-generation)<0)) return false; generation=next; return true; }
esp_err_t ui_mode_enter_normal(unsigned reason)
{ assert(reason==APP_NORMAL_LIVE); return ESP_OK; }
esp_err_t app_ui_show_jpeg(const uint8_t *jpeg,size_t length)
{ assert(jpeg==expected_jpeg && length==expected_length); ++shown; return ESP_OK; }
esp_err_t app_ui_recover_display(void)
{ assert(!"unexpected display recovery"); return ESP_FAIL; }

static void start(void)
{
    const app_endpoint_config_t queues={1,1};
    assert(app_console_router_start()==ESP_OK);
    assert(app_console_endpoint_register(APP_ENDPOINT_CAMERA,&queues)==ESP_OK);
    assert(app_console_endpoint_register(APP_ENDPOINT_UI,&queues)==ESP_OK);
    assert(app_console_endpoint_register(APP_ENDPOINT_UART,&queues)==ESP_OK);
    assert(app_console_subscribe(APP_MESSAGE_CAMERA_FRAME,APP_ENDPOINT_UI)==ESP_OK);
    assert(app_console_subscribe(APP_MESSAGE_CAMERA_FRAME,APP_ENDPOINT_UART)==ESP_OK);
    app_console_freeze_subscriptions();
}
static app_message_t receive(app_endpoint_t endpoint,app_message_type_t type)
{
    app_message_t message;
    assert(app_console_receive(endpoint,&message,0)==ESP_OK && message.type==type);
    return message;
}
static int publish(camera_frames_t *frames)
{
    int slot=camera_frames_acquire(frames); assert(slot>=0);
    expected_jpeg=frames->slots[slot].buffer+3; expected_length=13;
    assert(camera_frames_publish(frames,slot,expected_jpeg,expected_length,7)==ESP_OK);
    return slot;
}
int main(void)
{
    uint8_t buffers[2][32]={{0}};
    camera_frames_t frames;
    assert(camera_frames_init(&frames,7,buffers[0],buffers[1],sizeof buffers[0]));
    start();
    int slot=publish(&frames);
    /* A control message must pass an already queued JPEG. */
    app_message_t control={.type=APP_MESSAGE_UI_MENU_ACTION,.source=APP_ENDPOINT_CAMERA,
        .target=APP_ENDPOINT_UI,.generation=7};
    assert(app_console_send(&control)==ESP_OK);
    control=receive(APP_ENDPOINT_UI,APP_MESSAGE_UI_MENU_ACTION); app_message_release(&control);
    app_message_t ui=receive(APP_ENDPOINT_UI,APP_MESSAGE_CAMERA_FRAME);
    app_message_t observer=receive(APP_ENDPOINT_UART,APP_MESSAGE_CAMERA_FRAME);
    assert(ui.lease==observer.lease && app_message_lease_data(ui.lease,NULL)==expected_jpeg);
    assert(!app_message_lease_write(ui.lease,NULL));
    assert(ui_frames_handle(&ui)==ESP_OK && shown==1); app_message_release(&ui);
    app_message_t result=receive(APP_ENDPOINT_CAMERA,APP_MESSAGE_UI_FRAME_RESULT);
    assert(camera_frames_result(&frames,&result) && frames.shown==1); app_message_release(&result);
    assert(!atomic_load(&frames.slots[slot].returned) && !camera_frames_drained(&frames));
    app_message_release(&observer);
    assert(camera_frames_drained(&frames) && !app_message_lease_count());

    /* Fill Camera control queue: UI may return both JPEGs before it can send
     * results. Both real Camera slots remain unavailable until results arrive. */
    control=(app_message_t){.type=APP_MESSAGE_CAMERA_STATUS,.source=APP_ENDPOINT_UART,
        .target=APP_ENDPOINT_CAMERA,.generation=7};
    assert(app_console_send(&control)==ESP_OK);
    for (unsigned i=0;i<2;++i) {
        publish(&frames);
        ui=receive(APP_ENDPOINT_UI,APP_MESSAGE_CAMERA_FRAME);
        observer=receive(APP_ENDPOINT_UART,APP_MESSAGE_CAMERA_FRAME);
        assert(ui_frames_handle(&ui)==ESP_OK); app_message_release(&ui); app_message_release(&observer);
    }
    assert(!ui_frames_can_receive() && ui_frames_pending());
    assert(camera_frames_drained(&frames) && camera_frames_acquire(&frames)==-1);
    control=receive(APP_ENDPOINT_CAMERA,APP_MESSAGE_CAMERA_STATUS); app_message_release(&control);
    for (unsigned i=0;i<2;++i) {
        ui_frames_flush(); result=receive(APP_ENDPOINT_CAMERA,APP_MESSAGE_UI_FRAME_RESULT);
        assert(camera_frames_result(&frames,&result)); app_message_release(&result);
    }
    assert(!ui_frames_pending() && frames.shown==3 && !app_message_lease_count());

    /* Quiesce drains the queued observer, but cannot revoke UI's delivered
     * frame. UI returns its lease after the stopped router rejects the result. */
    publish(&frames); ui=receive(APP_ENDPOINT_UI,APP_MESSAGE_CAMERA_FRAME);
    assert(!app_console_router_quiesce(2));
    assert(!camera_frames_drained(&frames) && app_message_lease_count()==1);
    assert(ui_frames_handle(&ui)==ESP_OK && !ui_frames_pending());
    app_message_release(&ui);
    assert(camera_frames_drained(&frames) && app_console_router_quiesce(2));
    assert(!app_message_lease_count());

    /* Partial fanout: UI's full queue rejects a second frame while UART
     * accepts it. The failed publish must still pin the second buffer. */
    start(); assert(camera_frames_init(&frames,8,buffers[0],buffers[1],sizeof buffers[0]));
    publish(&frames); observer=receive(APP_ENDPOINT_UART,APP_MESSAGE_CAMERA_FRAME); app_message_release(&observer);
    slot=camera_frames_acquire(&frames); assert(slot>=0);
    assert(camera_frames_publish(&frames,slot,frames.slots[slot].buffer,9,2)==ESP_ERR_TIMEOUT);
    assert(!atomic_load(&frames.slots[slot].returned));
    observer=receive(APP_ENDPOINT_UART,APP_MESSAGE_CAMERA_FRAME);
    assert(app_message_lease_data(observer.lease,NULL)==frames.slots[slot].buffer);
    app_console_endpoint_stop(APP_ENDPOINT_UI);
    assert(!camera_frames_drained(&frames));
    app_message_release(&observer);
    assert(camera_frames_drained(&frames) && app_console_router_quiesce(2));
    assert(!app_message_lease_count());
    return 0;
}
