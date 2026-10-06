#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_console/display_bench.c"
static uint32_t uart_generation=2,ui_generation=3,sequence;
static int64_t clock_us=1000;
static esp_err_t transport,status_result=ESP_ERR_NOT_FOUND;
static bool sim_enabled;
static char output[4096];
int64_t esp_timer_get_time(void) { return clock_us; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { return endpoint==APP_ENDPOINT_UART ? uart_generation : ui_generation; }
uint32_t debug_async_token(void) { return ++sequence; }
const char *esp_err_to_name(esp_err_t err) { return err==ESP_OK ? "ESP_OK" : "ERROR"; }
int debug_printf(const char *format,...)
{
    va_list args;va_start(args,format);int n=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return n;
}
void app_message_release(app_message_t *message) { assert(!message->lease); }
esp_err_t app_console_request_many(app_message_t *requests,app_message_t *replies,esp_err_t *results,size_t count)
{
    assert(count==3 && requests[0].target==APP_ENDPOINT_INPUT && requests[1].target==APP_ENDPOINT_INPUT_SIM &&
        requests[1].payload.command.index==APP_INPUT_SIM_STATUS && requests[2].target==APP_ENDPOINT_SYSTEM);
    for(unsigned i=0;i<3;++i){results[i]=ESP_OK;replies[i].result=ESP_OK;}
    replies[1].payload.command.flag=sim_enabled;replies[2].payload.system.mode=APP_SYSTEM_MODE_NORMAL;return ESP_OK;
}
esp_err_t app_console_request(app_message_t *message,app_message_t *reply)
{
    assert(message->type==APP_MESSAGE_DISPLAY_BENCH && message->source==APP_ENDPOINT_UART && message->target==APP_ENDPOINT_UI &&
        message->flags==APP_MESSAGE_REQUEST && message->generation==uart_generation && message->deadline_us>clock_us);
    if(message->payload.command.index==APP_UI_BENCH_STATUS) { reply->result=status_result;reply->payload.benchmark.result=ESP_OK; }
    else { assert(message->payload.command.duration_ms==30000);reply->result=ESP_OK;reply->payload.command.token=message->payload.command.token; }
    return transport;
}
static void start(void)
{ output[0]=0;char *argv[]={"display","bench"};assert(display_bench_command(2,argv)); }
static app_message_t result(uint32_t value)
{ return (app_message_t){.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UI,.flags=APP_MESSAGE_EVENT,
    .generation=ui_generation,.endpoint_epoch=uart_generation,.result=ESP_OK,
    .payload.benchmark={.token=value,.frames=20,.bytes=4096,.elapsed_us=200000}}; }
int main(void)
{
    sim_enabled=true;start();assert(!token && strstr(output,"requires maintenance off"));sim_enabled=false;
    start();assert(token==1 && strstr(output,"queued token=1"));
    char *sim[]={"atom","sim","on"};output[0]=0;assert(display_bench_command(3,sim) && strstr(output,"cannot change"));
    app_message_t event=result(1);event.type=APP_MESSAGE_UI_PREFERENCES;display_bench_event(&event);assert(token==1);
    event=result(2);display_bench_event(&event);assert(token==1);
    event=result(1);display_bench_event(&event);assert(!token && strstr(output,"DONE display_bench"));
    output[0]=0;display_bench_event(&event);assert(!output[0]);
    transport=ESP_ERR_TIMEOUT;start();assert(token==2 && uncertain);
    transport=ESP_OK;clock_us+=200000;display_bench_poll();assert(!token && strstr(output,"FAIL display_bench"));
    transport=ESP_ERR_TIMEOUT;start();transport=ESP_OK;status_result=ESP_ERR_NOT_FINISHED;
    clock_us+=200000;display_bench_poll();assert(token==3);
    event=result(3);event.result=ESP_ERR_INVALID_STATE;display_bench_event(&event);assert(!token && strstr(output,"FAIL display_bench"));
    start();++ui_generation;display_bench_poll();assert(!token);
    puts("UART benchmark request, timeout resolution and event ownership passed");return 0;
}
