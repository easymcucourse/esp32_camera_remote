#include "ui_preferences_console.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include <string.h>
static struct { uint32_t token,uart_epoch,ui_epoch; unsigned operation; } pending[8];
static const char *info_name(unsigned value)
{ return value==0 ? "full" : value==1 ? "compact" : value==2 ? "hidden" : "unknown"; }
static void retire(void)
{
    for (unsigned i=0;i<8;++i) if (pending[i].token &&
        (pending[i].uart_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UART) ||
         pending[i].ui_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI))) pending[i].token=0;
}
void ui_preferences_event(const app_message_t *message)
{
    retire();
    if (message->type!=APP_MESSAGE_UI_PREFERENCES || message->source!=APP_ENDPOINT_UI ||
        message->flags!=APP_MESSAGE_EVENT || message->lease ||
        message->generation!=app_console_endpoint_generation(APP_ENDPOINT_UI) ||
        message->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UART)) return;
    for (unsigned i=0;i<8;++i) if (pending[i].token && pending[i].token==message->payload.command.token &&
        pending[i].operation==message->payload.command.index) {
        debug_printf("[dbg] %s ui token=%lu info=%s result=%s\n",message->result==ESP_OK ? "DONE" : "FAIL",
            (unsigned long)pending[i].token,info_name(message->payload.command.value),esp_err_to_name(message->result));
        pending[i].token=0;break;
    }
}
static esp_err_t request(unsigned op,unsigned value,bool admission,app_message_t *reply)
{
    app_message_t message={.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=app_console_endpoint_generation(APP_ENDPOINT_UART),
        .deadline_us=esp_timer_get_time()+500000,
        .payload.command={.index=op,.value=value,.flag=admission}};
    esp_err_t err=app_console_request(&message,reply);return err==ESP_OK ? reply->result : err;
}
bool ui_preferences_command(int argc,char **argv)
{
    if (!argc || strcmp(argv[0],"ui")) return false;
    retire();
    if (argc>=2 && !strcmp(argv[1],"pad")) {
        bool set=argc==3 && (!strcmp(argv[2],"ds") || !strcmp(argv[2],"xbox"));
        if (argc!=2 && !set) { debug_printf("[dbg] ERR ui pad [ds|xbox]\n");return true; }
        app_message_t reply={0};esp_err_t err=request(set ? APP_UI_PREF_PAD_SET : APP_UI_PREF_GET,
            set && !strcmp(argv[2],"xbox"),false,&reply);
        if (err==ESP_OK) debug_printf("[dbg] OK ui pad=%s%s\n",reply.payload.command.direction==0 ? "ds" : "xbox",set ? " result=ESP_OK" : "");
        else debug_printf("[dbg] ERR ui %s\n",esp_err_to_name(err));
        app_message_release(&reply);
        return true;
    }
    if (argc==2 && !strcmp(argv[1],"info")) {
        app_message_t reply={0};esp_err_t err=request(APP_UI_PREF_GET,0,false,&reply);
        if (err==ESP_OK) debug_printf("[dbg] OK ui info=%s\n",info_name(reply.payload.command.value));
        else debug_printf("[dbg] ERR ui %s\n",esp_err_to_name(err));
        app_message_release(&reply);return true;
    }
    unsigned value=argc==3?!strcmp(argv[2],"full")?0:!strcmp(argv[2],"compact")?1:!strcmp(argv[2],"hidden")?2:3:3;
    bool next=argc==3 && !strcmp(argv[2],"next");
    if (argc!=3 || strcmp(argv[1],"info") || (value>2 && !next)) {
        debug_printf("[dbg] ERR ui info [full|compact|hidden|next]\n");return true;
    }
    unsigned slot=0;while (slot<8 && pending[slot].token) ++slot;
    if (slot==8) { debug_printf("[dbg] ERR ui requests pending\n");return true; }
    pending[slot].uart_epoch=app_console_endpoint_generation(APP_ENDPOINT_UART);
    pending[slot].ui_epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);
    pending[slot].operation=next ? APP_UI_PREF_INFO_NEXT : APP_UI_PREF_INFO_SET;
    app_message_t reply={0};esp_err_t err=request(next ? APP_UI_PREF_INFO_NEXT : APP_UI_PREF_INFO_SET,value,true,&reply);
    uint32_t token=reply.payload.command.token;
    if (err==ESP_OK && !token) err=ESP_ERR_INVALID_RESPONSE;
    if (err!=ESP_OK) debug_printf("[dbg] ERR ui %s\n",esp_err_to_name(err));
    else { pending[slot].token=token;debug_printf("[dbg] OK ui queued token=%lu\n",(unsigned long)token); }
    app_message_release(&reply);
    return true;
}
