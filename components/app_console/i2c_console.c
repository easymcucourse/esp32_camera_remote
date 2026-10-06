#include "i2c_console.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include <string.h>
static uint32_t reported_dropped;
static bool logging;
static esp_err_t request(unsigned op,unsigned value,bool flag,app_message_t *reply)
{
    app_message_t m={.type=APP_MESSAGE_INPUT_ATOM_COMMAND,.source=APP_ENDPOINT_UART,
        .target=APP_ENDPOINT_INPUT_ATOM,.flags=APP_MESSAGE_REQUEST,
        .generation=app_console_endpoint_generation(APP_ENDPOINT_UART),
        .deadline_us=esp_timer_get_time()+100000};
    m.payload.command.index=op;m.payload.command.value=value;m.payload.command.flag=flag;
    esp_err_t err=app_console_request(&m,reply);return err==ESP_OK?reply->result:err;
}
bool i2c_console_command(int argc,char **argv)
{
    if (strcmp(argv[0],"i2c")) return false;
    if (argc==3 && !strcmp(argv[1],"log")) {
        unsigned mode=!strcmp(argv[2],"on")?I2C_LOG_ALL:!strcmp(argv[2],"off")?I2C_LOG_OFF:
            !strcmp(argv[2],"changes")?I2C_LOG_CHANGES:I2C_LOG_CHANGES+1;
        if (mode>I2C_LOG_CHANGES) { debug_printf("[dbg] ERR usage: i2c log on|off|changes\n");return true; }
        app_message_t reply={0};esp_err_t err=request(APP_INPUT_ATOM_LOG_MODE,mode,false,&reply);
        app_message_release(&reply);
        if (err==ESP_OK) { logging=mode!=I2C_LOG_OFF;debug_printf("[dbg] OK i2c log %s\n",argv[2]); }
        else debug_printf("[dbg] ERR i2c log %s\n",esp_err_to_name(err));
        return true;
    }
    if ((argc==2 || (argc==3 && !strcmp(argv[2],"reset"))) && !strcmp(argv[1],"stats")) {
        app_message_t reply={0};esp_err_t err=request(APP_INPUT_ATOM_STATS,0,argc==3,&reply);
        if (err==ESP_OK) {
            const i2c_monitor_stats_t *s=&reply.payload.i2c_stats;if(argc==3)reported_dropped=0;
            debug_printf("[dbg] OK i2c stats total=%lu failed=%lu timeout=%lu bad_crc=%lu bad_header=%lu bad_seq=%lu remote=%lu io=%lu bad_param=%lu max_ms=%lu log_dropped=%lu\n",
                (unsigned long)s->total,(unsigned long)s->failed,(unsigned long)s->counts[I2C_MON_TIMEOUT],
                (unsigned long)s->counts[I2C_MON_BAD_CRC],(unsigned long)s->counts[I2C_MON_BAD_HEADER],
                (unsigned long)s->counts[I2C_MON_BAD_SEQ],(unsigned long)s->counts[I2C_MON_REMOTE],
                (unsigned long)s->counts[I2C_MON_IO],(unsigned long)s->counts[I2C_MON_BAD_PARAM],
                (unsigned long)s->max_ms,(unsigned long)s->log_dropped);
        } else debug_printf("[dbg] ERR i2c stats %s\n",esp_err_to_name(err));
        app_message_release(&reply);return true;
    }
    return false;
}
static void hex(char *out,const uint8_t *bytes,unsigned count)
{
    static const char digits[]="0123456789abcdef";
    for(unsigned i=0;i<count;++i){out[3*i]=digits[bytes[i]>>4];out[3*i+1]=digits[bytes[i]&15];out[3*i+2]=' ';}
    out[3*count]=0;
}
void i2c_console_poll(void)
{
    if (!logging) return;
    for (unsigned i=0;i<2;++i) {
        app_message_t reply={0};esp_err_t err=request(APP_INPUT_ATOM_LOG_READ,0,false,&reply);
        if (err!=ESP_OK) { app_message_release(&reply);return; }
        if (reply.payload.i2c.dropped!=reported_dropped) {
            debug_printf("[dbg] i2c log dropped=%lu\n",(unsigned long)(reply.payload.i2c.dropped-reported_dropped));
            reported_dropped=reply.payload.i2c.dropped;
        }
        if (!reply.payload.i2c.available) { app_message_release(&reply);return; }
        i2c_frame_record_t r=reply.payload.i2c.frame;app_message_release(&reply);
        if (r.response_len>ATOM_RESPONSE_MAX || (unsigned)r.result>=I2C_MON_RESULT_COUNT) continue;
        char req[ATOM_REQUEST_SIZE*3+1],resp[ATOM_RESPONSE_MAX*3+1];
        hex(req,r.request,ATOM_REQUEST_SIZE);hex(resp,r.response,r.response_len);
        debug_printf("[dbg] i2c %lu #%02x %s > %s< %s%s %lums\n",(unsigned long)r.timestamp,
            r.request[2],r.request[3]==ATOM_CMD_POLL?"POLL":r.request[3]==ATOM_CMD_HELLO?"HELLO":"UNKNOWN",
            req,resp,i2c_monitor_result_name(r.result),(unsigned long)r.elapsed_ms);
    }
}
