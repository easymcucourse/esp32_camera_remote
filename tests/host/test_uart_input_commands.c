#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "app_message_internal.h"
#include "../../components/app_console/i2c_console.c"
#include "../../components/app_console/lcd_sim.c"

static app_message_t observed[32];
static unsigned requests,prints,reply_returns,log_reads;
static esp_err_t transport=ESP_OK,consumer=ESP_OK;
static bool log_available,sim_enabled,enable_fail;
static char output[8192];
static pad_sequence_t submitted;
static unsigned sequence_count;
int64_t esp_timer_get_time(void) { return 1000000; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ return endpoint==APP_ENDPOINT_UART?7:endpoint==APP_ENDPOINT_INPUT_SIM?9:5; }
uint32_t debug_async_token(void) { return 123; }
const char *esp_err_to_name(esp_err_t error) { return error==ESP_OK?"ESP_OK":"ERROR"; }
int debug_printf(const char *format,...)
{
    ++prints;va_list args;va_start(args,format);
    int result=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return result;
}
static void reply_returned(void *context) { assert(!context);++reply_returns; }
esp_err_t app_console_request(app_message_t *m,app_message_t *reply)
{
    assert(requests<32 && m->source==APP_ENDPOINT_UART && m->generation==7);
    assert(m->flags==APP_MESSAGE_REQUEST || m->flags==(APP_MESSAGE_REQUEST|APP_MESSAGE_BULK));
    assert(m->deadline_us==1000000+(m->target==APP_ENDPOINT_INPUT_ATOM?100000:500000));
    observed[requests++]=*m;
    if(m->lease) {
        size_t length=0;const pad_sequence_t *seq=app_message_lease_data(m->lease,&length);
        assert(m->target==APP_ENDPOINT_INPUT_SIM && m->payload.command.index==APP_INPUT_SIM_SEQUENCE);
        assert(length==sizeof(*seq) && !app_message_lease_write(m->lease,NULL));
        submitted=*seq;++sequence_count;
    }
    app_message_release(m); /* Request consumes even on transport failure. */
    if(transport!=ESP_OK)return transport;
    *reply=(app_message_t){.result=consumer};
    if(enable_fail && m->target==APP_ENDPOINT_INPUT_SIM && m->payload.command.index==APP_INPUT_SIM_ENABLE)
        reply->result=ESP_ERR_INVALID_STATE;
    if(m->target==APP_ENDPOINT_INPUT_ATOM && m->payload.command.index==APP_INPUT_ATOM_LOG_READ) {
        ++log_reads;reply->payload.i2c.available=log_available;log_available=false;
        reply->payload.i2c.dropped=3;
        reply->payload.i2c.frame=(i2c_frame_record_t){.response_len=1,.result=I2C_MON_OK};
    } else reply->payload.command.flag=sim_enabled;
    static unsigned char value;
    assert(app_message_lease_create(&value,1,false,reply_returned,NULL,&reply->lease)==ESP_OK);
    return ESP_OK;
}
static void reset(void)
{ assert(!app_message_lease_count());requests=prints=reply_returns=log_reads=0;output[0]=0;transport=consumer=ESP_OK;enable_fail=false; }
int main(void)
{
    char *unrelated[]={"maint"};
    assert(!i2c_console_command(1,unrelated) && !lcd_sim_command(1,unrelated));
    char *changes[]={"i2c","log","changes"};
    assert(i2c_console_command(3,changes));
    assert(requests==1 && observed[0].payload.command.index==APP_INPUT_ATOM_LOG_MODE &&
        observed[0].payload.command.value==I2C_LOG_CHANGES && logging && reply_returns==1);
    log_available=true;i2c_console_poll();assert(log_reads==2 && reply_returns==3);
    assert(strstr(output,"i2c log dropped=3") && strstr(output,"OK"));
    char *off[]={"i2c","log","off"};
    assert(i2c_console_command(3,off));unsigned before=requests;i2c_console_poll();assert(requests==before);
    char *stats[]={"i2c","stats","reset"};
    assert(i2c_console_command(3,stats) && observed[requests-1].payload.command.flag && !reported_dropped);
    char *badlog[]={"i2c","log","invalid"};before=requests;
    assert(i2c_console_command(3,badlog) && requests==before);

    reset();char *on[]={"atom","sim","on"};
    consumer=ESP_ERR_INVALID_STATE;assert(lcd_sim_command(3,on) && requests==1 && reply_returns==1);
    assert(observed[0].type==APP_MESSAGE_INPUT_SELECT && observed[0].target==APP_ENDPOINT_INPUT);
    reset();transport=ESP_ERR_TIMEOUT;assert(lcd_sim_command(3,on) && requests==1 && !reply_returns);
    reset();assert(lcd_sim_command(3,on) && requests==2 && reply_returns==2);
    assert(observed[0].payload.command.index==1 && observed[1].payload.command.index==APP_INPUT_SIM_ENABLE &&
        observed[1].payload.command.value==1);
    reset();enable_fail=true;assert(lcd_sim_command(3,on) && requests==2 && reply_returns==2 && strstr(output,"ERR SIM input selection"));
    reset();
    sim_enabled=true;assert(lcd_sim_enabled());sim_enabled=false;assert(!lcd_sim_enabled());
    char *battery[]={"pad","battery","none"};assert(lcd_sim_command(3,battery));
    assert(observed[requests-1].payload.command.index==APP_INPUT_SIM_BATTERY && observed[requests-1].payload.command.value==255);
    char *badbattery[]={"pad","battery","11"};before=requests;
    assert(lcd_sim_command(3,badbattery) && requests==before);
    char *negative[]={"atom","timeout","-1"};assert(lcd_sim_command(3,negative) && requests==before);
    char *gap[]={"pad","gap"};assert(lcd_sim_command(2,gap));
    assert(observed[requests-1].payload.command.index==APP_INPUT_SIM_GAP);

    reset();char *tap[]={"tap","a"};assert(lcd_sim_command(2,tap));
    assert(sequence_count==1 && requests==1 && reply_returns==1 && observed[0].payload.command.token==123);
    assert(submitted.count && strstr(output,"queued token=123") && !app_message_lease_count());
    assert(submitted.actions[0].kind==PAD_PRESS && submitted.actions[0].mask==pad_cmd_button_mask("a") &&
        submitted.actions[submitted.count-1].kind==PAD_RELEASE);
    reset();transport=ESP_ERR_TIMEOUT;assert(lcd_sim_command(2,tap));
    assert(sequence_count==2 && requests==1 && !reply_returns && !app_message_lease_count() && strstr(output,"ERR SIM queue"));
    reset();char *badtap[]={"tap","unknown"};assert(lcd_sim_command(2,badtap) && !requests);
    app_message_t held[APP_MESSAGE_LEASE_CAPACITY]={0};static unsigned char borrowed;
    for(unsigned i=0;i<APP_MESSAGE_LEASE_CAPACITY;++i)
        assert(app_message_lease_create(&borrowed,1,false,reply_returned,NULL,&held[i].lease)==ESP_OK);
    assert(lcd_sim_command(2,tap) && !requests && strstr(output,"ERR SIM memory"));
    assert(app_message_lease_count()==APP_MESSAGE_LEASE_CAPACITY);
    for(unsigned i=0;i<APP_MESSAGE_LEASE_CAPACITY;++i)app_message_release(&held[i]);
    app_message_t event={.type=APP_MESSAGE_INPUT_SIM_COMMAND,.source=APP_ENDPOINT_INPUT_SIM,
        .flags=APP_MESSAGE_EVENT,.generation=9,.endpoint_epoch=7,
        .payload.command={.index=APP_INPUT_SIM_SEQUENCE,.token=123,.duration_ms=50}};
    before=prints;lcd_sim_event(&event);assert(prints==before+1 && strstr(output,"DONE SIM pad token=123"));
    before=prints;--event.generation;lcd_sim_event(&event);assert(prints==before);
    ++event.generation;--event.endpoint_epoch;lcd_sim_event(&event);assert(prints==before);
    assert(!app_message_lease_count());return 0;
}
