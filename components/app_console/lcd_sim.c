#include "lcd_sim.h"
#include "debug_console.h"
#include "app_console.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include <stdlib.h>
#include <string.h>
#if CONFIG_REMOTE_DBG_SIM
static app_message_t message(app_message_type_t type,app_endpoint_t target)
{
    return (app_message_t){.type=type,.source=APP_ENDPOINT_UART,.target=target,.flags=APP_MESSAGE_REQUEST,
        .generation=app_console_endpoint_generation(APP_ENDPOINT_UART),.deadline_us=esp_timer_get_time()+500000};
}
static esp_err_t scalar(unsigned op,uint32_t value,app_message_t *reply)
{
    app_message_t m=message(APP_MESSAGE_INPUT_SIM_COMMAND,APP_ENDPOINT_INPUT_SIM);
    m.payload.command.index=op;m.payload.command.value=value;
    esp_err_t err=app_console_request(&m,reply);return err==ESP_OK?reply->result:err;
}
bool lcd_sim_enabled(void)
{
    app_message_t reply={0};esp_err_t err=scalar(APP_INPUT_SIM_STATUS,0,&reply);
    bool enabled=err==ESP_OK && reply.payload.command.flag;app_message_release(&reply);return enabled;
}
void lcd_sim_event(const app_message_t *message)
{
    app_message_t m=*message;
        if (m.type==APP_MESSAGE_INPUT_SIM_COMMAND && m.source==APP_ENDPOINT_INPUT_SIM &&
            m.flags==APP_MESSAGE_EVENT && !m.lease && m.generation &&
            m.generation==app_console_endpoint_generation(APP_ENDPOINT_INPUT_SIM) &&
            m.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
            m.payload.command.index==APP_INPUT_SIM_SEQUENCE)
            debug_printf("[dbg] %s SIM pad token=%lu elapsed=%lums\n",m.payload.command.flag?"FAIL":"DONE",
                (unsigned long)m.payload.command.token,(unsigned long)m.payload.command.duration_ms);
}
static bool number(const char *text,unsigned maximum,uint32_t *out)
{
    char *end;unsigned long n=strtoul(text,&end,10);
    if (!*text || *end || text[0]=='-' || n>maximum) return false;
    *out=n;return true;
}
static void returned(void *context) { free(context); }
bool lcd_sim_command(int argc,char **argv)
{
    bool atom=!strcmp(argv[0],"atom"),pad=!strcmp(argv[0],"pad"),gimbal=!strcmp(argv[0],"gimbal");
    if (!atom && !pad && !gimbal && strcmp(argv[0],"tap") && strcmp(argv[0],"hold") &&
        strcmp(argv[0],"release") && strcmp(argv[0],"stick") && strcmp(argv[0],"trigger") &&
        strcmp(argv[0],"shoot") && strcmp(argv[0],"record") && strcmp(argv[0],"seq")) return false;
    unsigned op=APP_INPUT_SIM_STATUS;uint32_t value=0;bool valid=false;
    if (atom && argc==3 && !strcmp(argv[1],"sim") && (!strcmp(argv[2],"on") || !strcmp(argv[2],"off"))) {
        value=!strcmp(argv[2],"on");
        app_message_t m=message(APP_MESSAGE_INPUT_SELECT,APP_ENDPOINT_INPUT),reply={0};
        m.payload.command.index=value?1:0;
        esp_err_t err=app_console_request(&m,&reply);if (err==ESP_OK) err=reply.result;app_message_release(&reply);
        if (err==ESP_OK) { err=scalar(APP_INPUT_SIM_ENABLE,value,&reply);app_message_release(&reply); }
        if (err!=ESP_OK) debug_printf("[dbg] ERR SIM input selection %s\n",esp_err_to_name(err));
        else debug_printf("[dbg] OK SIM atom sim %s requested RAM only\n",argv[2]);
        return true;
    }
    if (atom) {
        if (argc==2 && (!strcmp(argv[1],"online") || !strcmp(argv[1],"offline"))) {
            op=APP_INPUT_SIM_ONLINE;value=!strcmp(argv[1],"online");valid=true;
        } else if (argc==2 && !strcmp(argv[1],"reboot")) { op=APP_INPUT_SIM_REBOOT;valid=true; }
        else if (argc==3 && !strcmp(argv[1],"version")) { op=APP_INPUT_SIM_VERSION;valid=number(argv[2],255,&value); }
        else if (argc==3 && (!strcmp(argv[1],"fail") || !strcmp(argv[1],"crc") || !strcmp(argv[1],"timeout"))) {
            op=!strcmp(argv[1],"fail")?APP_INPUT_SIM_FAIL:!strcmp(argv[1],"crc")?APP_INPUT_SIM_CRC:APP_INPUT_SIM_TIMEOUT;
            valid=number(argv[2],10000,&value);
        }
    } else if (gimbal) {
        op=APP_INPUT_SIM_GIMBAL;
        value=argc==3 && !strcmp(argv[1],"state")?(!strcmp(argv[2],"off")?0:!strcmp(argv[2],"search")?1:!strcmp(argv[2],"connecting")?2:!strcmp(argv[2],"connected")?3:4):4;
        valid=value<=3;
    } else if (pad) {
        if (argc==2 && (!strcmp(argv[1],"connect") || !strcmp(argv[1],"disconnect"))) {
            op=APP_INPUT_SIM_CONNECT;value=!strcmp(argv[1],"connect");valid=true;
        } else if (argc==3 && !strcmp(argv[1],"battery")) {
            op=APP_INPUT_SIM_BATTERY;
            valid=!strcmp(argv[2],"none") || number(argv[2],10,&value);
            if (!strcmp(argv[2],"none")) value=255;
        } else if (argc==2 && (!strcmp(argv[1],"gap") || !strcmp(argv[1],"overflow"))) {
            op=!strcmp(argv[1],"gap")?APP_INPUT_SIM_GAP:APP_INPUT_SIM_OVERFLOW;valid=true;
        }
    } else {
        pad_sequence_t sequence;const char *error;
        if (!pad_cmd_parse(argc,argv,&sequence,&error)) { debug_printf("[dbg] ERR SIM %s\n",error);return true; }
        pad_sequence_t *copy=malloc(sizeof(*copy));
        if (!copy) { debug_printf("[dbg] ERR SIM memory\n");return true; }
        *copy=sequence;
        app_message_t m=message(APP_MESSAGE_INPUT_SIM_COMMAND,APP_ENDPOINT_INPUT_SIM),reply={0};
        m.flags|=APP_MESSAGE_BULK;m.payload.command.index=APP_INPUT_SIM_SEQUENCE;
        m.payload.command.token=debug_async_token();uint32_t token=m.payload.command.token;
        esp_err_t err=app_message_lease_create(copy,sizeof(*copy),false,returned,copy,&m.lease);
        if (err!=ESP_OK) { free(copy);debug_printf("[dbg] ERR SIM memory\n");return true; }
        err=app_console_request(&m,&reply);if (err==ESP_OK) err=reply.result;
        if (err!=ESP_OK) debug_printf("[dbg] ERR SIM queue %s\n",esp_err_to_name(err));
        else {
            if (sequence.camera_warning) debug_printf("[dbg] SIM WARN camera will receive shutter/record\n");
            debug_printf("[dbg] OK SIM queued token=%lu duration=%lums\n",(unsigned long)token,(unsigned long)reply.payload.command.duration_ms);
        }
        app_message_release(&reply);return true;
    }
    if (!valid) { debug_printf("[dbg] ERR SIM invalid command; see help\n");return true; }
    app_message_t reply={0};esp_err_t err=scalar(op,value,&reply);
    if (err==ESP_OK) debug_printf("[dbg] OK SIM %s %s value=%lu\n",argv[0],argc>1?argv[1]:"",(unsigned long)value);
    else debug_printf("[dbg] ERR SIM %s\n",esp_err_to_name(err));
    app_message_release(&reply);return true;
}
#endif
