#include "ui_mode_messages.h"
#include "app_console.h"
#include <assert.h>
static uint32_t epoch=3;
static unsigned requested,released;
static esp_err_t transport=ESP_ERR_TIMEOUT,decision=ESP_ERR_INVALID_STATE;
int64_t esp_timer_get_time(void) { return 100; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_UI);return epoch; }
esp_err_t app_console_request(app_message_t *request,app_message_t *reply)
{
    assert(request->type==APP_MESSAGE_SYSTEM_ENTER_NORMAL && request->source==APP_ENDPOINT_UI && request->target==APP_ENDPOINT_SYSTEM &&
        request->generation==epoch && request->flags==APP_MESSAGE_REQUEST && request->deadline_us==1000100 && !request->lease &&
        request->payload.command.index<=APP_NORMAL_DISPLAY_TEST);
    ++requested;reply->result=decision;return transport;
}
void app_message_release(app_message_t *message) { assert(!message->lease);++released; }
int main(void)
{
    assert(ui_mode_enter_normal(99)==ESP_ERR_INVALID_ARG && !requested);
    assert(ui_mode_enter_normal(APP_NORMAL_LIVE)==ESP_ERR_TIMEOUT && requested==1 && released==1);
    transport=ESP_OK;assert(ui_mode_enter_normal(APP_NORMAL_SETTINGS)==ESP_ERR_INVALID_STATE && requested==2 && released==2);
    decision=ESP_OK;assert(ui_mode_enter_normal(APP_NORMAL_LIVE)==ESP_OK && requested==3 && released==3);
    assert(ui_mode_enter_normal(APP_NORMAL_LIVE)==ESP_OK && requested==3);
    ++epoch;assert(ui_mode_enter_normal(APP_NORMAL_SETTINGS)==ESP_OK && requested==4 && released==4);
    epoch=0;assert(ui_mode_enter_normal(APP_NORMAL_LIVE)==ESP_ERR_INVALID_STATE && requested==4);
    return 0;
}
