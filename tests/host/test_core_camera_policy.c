#include "app_core.h"
#include "app_core_services.h"
#include "app_core_camera.h"
#include "app_core_mode.h"
#include "app_camera.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <assert.h>
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t err) { (void)err;return "fake"; }
static unsigned inits,messages,consoles,starts;
static unsigned message_stops;
static bool message_failure=true,console_failure=true;
static uint32_t api_version=APP_CAMERA_API_VERSION;
uint32_t app_camera_api_version(void) { return api_version; }
static int64_t clock_us;
esp_err_t app_camera_init(const app_camera_config_t *config)
{ assert(config->backend==APP_CAMERA_BACKEND_DEFAULT); ++inits; return ESP_OK; }
esp_err_t app_camera_messages_start(void) { ++messages; return message_failure ? ESP_ERR_NO_MEM : ESP_OK; }
esp_err_t app_camera_start(app_camera_start_mode_t mode) { assert(mode==APP_CAMERA_START_PREVIEW); ++starts; return ESP_OK; }
bool app_camera_stop(uint32_t timeout) { return timeout==3000; }
bool app_camera_messages_quiesce(uint32_t timeout) { ++message_stops;return timeout==1000; }
esp_err_t app_console_uart_start(void) { ++consoles;return console_failure?ESP_FAIL:ESP_OK; }
int64_t esp_timer_get_time(void) { return clock_us; }
void vTaskDelay(TickType_t delay) { clock_us+=(int64_t)delay*1000; }
int main(void)
{
    app_message_t request={.source=APP_ENDPOINT_CAMERA,.flags=APP_MESSAGE_REQUEST,.generation=7,.deadline_us=3000000};
    request.payload.command.flag=true;
    assert(app_core_camera_session(&request)==ESP_ERR_INVALID_STATE);
    --api_version;assert(app_core_camera_boot()==ESP_ERR_NOT_SUPPORTED && !inits && !messages && !consoles && !starts);++api_version;
    assert(app_core_camera_boot()==ESP_ERR_NO_MEM && inits==1 && messages==1 && !consoles && !starts);
    message_failure=false; assert(app_core_camera_boot()==ESP_FAIL && inits==1 && messages==2 && consoles==1 && starts==0);
    assert(!app_core_console_ready());
    assert(app_core_camera_session(&request)==ESP_OK && clock_us==0);
    request.source=APP_ENDPOINT_UI; assert(app_core_camera_session(&request)==ESP_ERR_INVALID_ARG);
    request.source=APP_ENDPOINT_CAMERA; request.deadline_us=clock_us;
    assert(app_core_camera_session(&request)==ESP_ERR_TIMEOUT);
    request.deadline_us=clock_us+3000000;request.payload.command.flag=false;request.flags=0;
    assert(app_core_camera_session(&request)==ESP_OK);
    request.payload.command.flag=true;assert(app_core_camera_session(&request)==ESP_ERR_INVALID_ARG);
    assert(app_core_camera_quiesce(3000) && !app_core_camera_quiesce(1));
    /* UART creation failure above did not suppress Camera start. Retry later
     * composes UART without recreating existing Camera endpoints. */
    console_failure=false;
    assert(app_core_camera_boot()==ESP_OK && consoles==2 && starts==1 && messages==2);
    assert(app_core_console_ready());
    assert(app_core_camera_boot()==ESP_OK && consoles==2 && starts==2 && messages==2);
    assert(!app_core_camera_messages_quiesce(0) && message_stops==1);
    assert(app_core_camera_messages_quiesce(1000) && message_stops==2);
    assert(app_core_camera_messages_quiesce(0) && message_stops==2);
    assert(app_core_camera_boot()==ESP_OK && messages==3 && starts==3 && consoles==2);
    assert(app_core_mode_request_maintenance(&app_core_mode));
    app_core_mode_boot_begin(&app_core_mode);
    assert(app_core_camera_boot()==ESP_OK && starts==3 && messages==3);
    app_core_mode_boot_end(&app_core_mode);
    assert(app_core_camera_boot()==ESP_ERR_INVALID_STATE && starts==3 && messages==3);
    request.flags=APP_MESSAGE_REQUEST;request.payload.command.flag=true;
    assert(app_core_camera_session(&request)==ESP_ERR_INVALID_STATE);
    request.flags=0;request.payload.command.flag=false;
    assert(app_core_camera_session(&request)==ESP_OK);
    app_core_mode_restart(&app_core_mode);request.flags=APP_MESSAGE_REQUEST;request.payload.command.flag=true;
    assert(app_core_camera_session(&request)==ESP_ERR_INVALID_STATE);
    return 0;
}
