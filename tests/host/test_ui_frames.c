#include "ui_frames.h"
#include "app_console.h"
#include "app_ui_internal.h"
#include <assert.h>
static unsigned shown,recovered,returned,sent;
static uint32_t generation,last_token;
static esp_err_t display_result,recovery_result,send_result,last_result,mode_result;
bool ui_camera_generation_accept(uint32_t next)
{
    if (!next || (generation && (int32_t)(next-generation)<0)) return false;
    generation=next; return true;
}
esp_err_t app_ui_show_jpeg(const uint8_t *jpeg,size_t size)
{ assert(jpeg && size); ++shown; return display_result; }
esp_err_t app_ui_recover_display(void) { ++recovered; return recovery_result; }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->type==APP_MESSAGE_UI_FRAME_RESULT && message->source==APP_ENDPOINT_UI);
    assert(message->target==APP_ENDPOINT_CAMERA && !message->lease);
    if (send_result!=ESP_OK) return send_result;
    ++sent; last_result=message->result; last_token=message->payload.command.token; return ESP_OK;
}
static void release(void *context) { assert(!context); ++returned; }
static app_message_t make(uint32_t next,uint32_t token,bool writable)
{
    static uint8_t jpeg[8];
    app_message_t message={.source=APP_ENDPOINT_CAMERA,.type=APP_MESSAGE_CAMERA_FRAME,.generation=next};
    message.payload.command.token=token;
    assert(app_message_lease_create(jpeg,sizeof jpeg,writable,release,NULL,&message.lease)==ESP_OK);
    return message;
}
int main(void)
{
    app_message_t message=make(5,1,false);
    assert(ui_frames_handle(&message)==ESP_OK && shown==1 && sent==1 && last_token==1);
    assert(!returned); app_message_release(&message); assert(returned==1);
    display_result=ESP_ERR_INVALID_RESPONSE; message=make(5,2,false);
    assert(ui_frames_handle(&message)==ESP_ERR_INVALID_RESPONSE && last_result==ESP_ERR_INVALID_RESPONSE);
    app_message_release(&message);
    display_result=ESP_ERR_INVALID_STATE; recovery_result=ESP_OK; message=make(5,3,false);
    assert(ui_frames_handle(&message)==ESP_ERR_NOT_FINISHED && recovered==1 && last_result==ESP_ERR_NOT_FINISHED);
    app_message_release(&message); recovery_result=ESP_FAIL; message=make(5,4,false);
    assert(ui_frames_handle(&message)==ESP_FAIL && last_result==ESP_FAIL); app_message_release(&message);
    unsigned before=shown; message=make(4,5,false);
    assert(ui_frames_handle(&message)==ESP_ERR_INVALID_STATE && shown==before); app_message_release(&message);
    message=make(9,6,true);
    assert(ui_frames_handle(&message)==ESP_ERR_INVALID_ARG && generation==5); app_message_release(&message);
    display_result=ESP_OK; send_result=ESP_ERR_TIMEOUT;
    message=make(5,7,false); assert(ui_frames_handle(&message)==ESP_OK); app_message_release(&message);
    message=make(5,8,false); assert(ui_frames_handle(&message)==ESP_OK); app_message_release(&message);
    assert(!ui_frames_can_receive()); before=sent;
    ui_frames_flush(); assert(sent==before && !ui_frames_can_receive());
    send_result=ESP_OK; ui_frames_flush(); assert(sent==before+2 && ui_frames_can_receive() && last_token==8);
    send_result=ESP_ERR_INVALID_STATE; message=make(5,9,false);
    assert(ui_frames_handle(&message)==ESP_OK && ui_frames_can_receive()); app_message_release(&message);
    assert(returned==9);
    before=shown;send_result=ESP_ERR_TIMEOUT;message=make(5,10,false);
    assert(ui_frames_drop(&message,ESP_ERR_INVALID_STATE)==ESP_ERR_INVALID_STATE && shown==before && ui_frames_pending());
    app_message_release(&message);assert(returned==10);
    send_result=ESP_OK;ui_frames_flush();assert(!ui_frames_pending() && last_token==10 && last_result==ESP_ERR_INVALID_STATE);
    mode_result=ESP_ERR_INVALID_STATE;before=shown;unsigned recovery_before=recovered;
    message=make(5,11,false);
    assert(ui_frames_handle(&message)==ESP_ERR_INVALID_STATE && shown==before && recovered==recovery_before && last_token==11);
    app_message_release(&message);assert(returned==11 && !ui_frames_pending());
    return 0;
}

esp_err_t ui_mode_enter_normal(unsigned reason) { assert(reason==APP_NORMAL_LIVE);return mode_result; }
