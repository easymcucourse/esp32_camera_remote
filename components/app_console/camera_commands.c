#include "uart_camera_commands.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <string.h>

static esp_err_t request(app_endpoint_t target,app_message_type_t type,
    app_message_payload_t payload,int64_t deadline)
{
    app_message_t message={.type=type,.source=APP_ENDPOINT_UART,.target=target,
        .flags=APP_MESSAGE_REQUEST,.generation=app_console_endpoint_generation(APP_ENDPOINT_UART),
        .deadline_us=deadline,.payload=payload},reply={0};
    esp_err_t err=app_console_request(&message,&reply);
    if (err==ESP_OK) err=reply.result;
    app_message_release(&reply);return err;
}
bool camera_commands_command(int argc,char **argv)
{
    if (!argc) return false;
    int64_t deadline=esp_timer_get_time()+500000;
#if CONFIG_REMOTE_DBG_SIM
    if (!strcmp(argv[0],"display")) {
        unsigned mode=3;
        if (argc==3 && !strcmp(argv[1],"fault"))
            mode=!strcmp(argv[2],"off") ? 0 : !strcmp(argv[2],"once") ? 1 : !strcmp(argv[2],"persistent") ? 2 : 3;
        if (mode==3) { debug_printf("[dbg] ERR usage: display fault off|once|persistent\n");return true; }
        esp_err_t err=request(APP_ENDPOINT_UI,APP_MESSAGE_DISPLAY_FAULT,
            (app_message_payload_t){.command={.value=mode}},deadline);
        if (err==ESP_OK) debug_printf("[dbg] OK display fault %s (RAM only)\n",argv[2]);
        else debug_printf("[dbg] ERR display fault %s\n",esp_err_to_name(err));
        return true;
    }
#endif
    if (argc!=1 || strlen(argv[0])!=1 || !strchr("pPjJSs",argv[0][0])) return false;
    char key=argv[0][0];esp_err_t err;
    if (key=='S') {
        err=request(APP_ENDPOINT_UI,APP_MESSAGE_UI_MENU_ACTION,
            (app_message_payload_t){.action={.type=PAD_ACTION_UI_TOGGLE}},deadline);
        /* Cancel MF even if UI acknowledgement failed; it may have toggled.
         * Safety cancellation has its own bounded deadline. */
        esp_err_t cancelled=request(APP_ENDPOINT_CAMERA,APP_MESSAGE_CAMERA_ACTION,
            (app_message_payload_t){.action={.type=PAD_ACTION_MF_CANCEL}},esp_timer_get_time()+500000);
        if (err==ESP_OK) err=cancelled;
    } else {
        app_message_type_t type=key=='s' ? APP_MESSAGE_CAMERA_STOP : APP_MESSAGE_CAMERA_START;
        bool flag=key=='s' || key=='p' || key=='P';
        err=request(APP_ENDPOINT_CAMERA,type,(app_message_payload_t){.command={.flag=flag}},deadline);
    }
    if (err==ESP_OK) debug_printf("[dbg] OK %c\n",key);
    else debug_printf("[dbg] ERR %c %s\n",key,esp_err_to_name(err));
    return true;
}
