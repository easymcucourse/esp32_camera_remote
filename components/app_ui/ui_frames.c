#include "ui_frames.h"
#include "ui_mode_messages.h"
#include "ui_camera_messages.h"
#include "app_console.h"
#include "app_ui_internal.h"
static app_message_t pending[2];
static unsigned head,count;
bool ui_frames_can_receive(void) { return count<2; }
bool ui_frames_pending(void) { return count!=0; }
void ui_frames_flush(void)
{
    while (count) {
        app_message_t result=pending[head];
        esp_err_t error=app_console_send(&result);
        if (error==ESP_ERR_TIMEOUT) return;
        /* Stopped router/Camera invalidates this delivery; its lease has
         * already returned. A new lifetime uses a different generation. */
        head=(head+1)%2; --count;
    }
}
esp_err_t ui_frames_drop(const app_message_t *message,esp_err_t error)
{
    if (!message || message->type!=APP_MESSAGE_CAMERA_FRAME || message->source!=APP_ENDPOINT_CAMERA ||
        !message->generation || !message->payload.command.token || count==2) return ESP_ERR_INVALID_ARG;
    app_message_t result={.type=APP_MESSAGE_UI_FRAME_RESULT,.source=APP_ENDPOINT_UI,
        .target=APP_ENDPOINT_CAMERA,.generation=message->generation,.result=error};
    result.payload.command.token=message->payload.command.token;
    pending[(head+count)%2]=result; ++count;
    ui_frames_flush();return error;
}
esp_err_t ui_frames_handle(const app_message_t *message)
{
    if (!message || message->type!=APP_MESSAGE_CAMERA_FRAME ||
        message->source!=APP_ENDPOINT_CAMERA || !message->generation ||
        !message->payload.command.token || count==2) return ESP_ERR_INVALID_ARG;
    size_t size=0;
    const uint8_t *jpeg=app_message_lease_data(message->lease,&size);
    esp_err_t error;
    if (!jpeg || !size || app_message_lease_write(message->lease,NULL)) error=ESP_ERR_INVALID_ARG;
    else if (!ui_camera_generation_accept(message->generation)) error=ESP_ERR_INVALID_STATE;
    else {
        error=ui_mode_enter_normal(APP_NORMAL_LIVE);
        if(error!=ESP_OK)return ui_frames_drop(message,error);
        error=app_ui_show_jpeg(jpeg,size);
        if (error==ESP_ERR_INVALID_STATE) {
            error=app_ui_recover_display();
            if (error==ESP_OK) error=ESP_ERR_NOT_FINISHED; /* Recovered, frame was dropped. */
        }
    }
    return ui_frames_drop(message,error);
}
