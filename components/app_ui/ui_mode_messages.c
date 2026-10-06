#include "ui_mode_messages.h"
#include "app_console.h"
#include "esp_timer.h"
/* Single UI message-task caller. Success is irreversible for this boot; cache
 * by UI endpoint lifetime so JPEG frames never issue per-frame Core RPCs. */
static uint32_t accepted_epoch;
esp_err_t ui_mode_enter_normal(unsigned reason)
{
    if(reason>APP_NORMAL_DISPLAY_TEST)return ESP_ERR_INVALID_ARG;
    uint32_t epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);
    if(!epoch)return ESP_ERR_INVALID_STATE;
    if(accepted_epoch==epoch)return ESP_OK;
    app_message_t request={.type=APP_MESSAGE_SYSTEM_ENTER_NORMAL,.source=APP_ENDPOINT_UI,.target=APP_ENDPOINT_SYSTEM,
        .flags=APP_MESSAGE_REQUEST,.generation=epoch,.deadline_us=esp_timer_get_time()+1000000},reply={0};
    request.payload.command.index=reason;
    esp_err_t result=app_console_request(&request,&reply);
    if(result==ESP_OK)result=reply.result;
    app_message_release(&reply);
    if(result==ESP_OK)accepted_epoch=epoch;
    return result;
}
