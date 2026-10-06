#include "ui_preferences_console.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include <string.h>
static const char *info_name(unsigned value)
{ return value==0 ? "full" : value==1 ? "compact" : value==2 ? "hidden" : "unknown"; }
bool ui_preferences_command(int argc,char **argv)
{
    if (!argc || strcmp(argv[0],"ui")) return false;
    if (argc!=2 || (strcmp(argv[1],"info") && strcmp(argv[1],"pad"))) {
        debug_printf("[dbg] ERR ui settings are changed in maintenance Web; use ui info or ui pad to query\n");return true;
    }
    app_message_t message={.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=app_console_endpoint_generation(APP_ENDPOINT_UART),
        .deadline_us=esp_timer_get_time()+500000,.payload.command={.index=APP_UI_PREF_GET}},reply={0};
    esp_err_t error=app_console_request(&message,&reply);
    if (error==ESP_OK) error=reply.result;
    if (error!=ESP_OK) debug_printf("[dbg] ERR ui %s\n",esp_err_to_name(error));
    else if (!strcmp(argv[1],"pad")) debug_printf("[dbg] OK ui pad=%s\n",reply.payload.command.direction==0 ? "ds" : "xbox");
    else debug_printf("[dbg] OK ui info=%s\n",info_name(reply.payload.command.value));
    app_message_release(&reply);return true;
}
