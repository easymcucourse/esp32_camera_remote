#include "display_bench.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include <string.h>
static uint32_t token,uart_epoch,ui_epoch;
static bool uncertain;
static int64_t next_poll;
static app_message_t checks[3],snapshots[3];
static esp_err_t check_results[3];
static esp_err_t preflight(void)
{
    const app_endpoint_t targets[]={APP_ENDPOINT_INPUT,APP_ENDPOINT_INPUT_SIM,APP_ENDPOINT_SYSTEM};
    const app_message_type_t types[]={APP_MESSAGE_INPUT_STATUS,APP_MESSAGE_INPUT_SIM_COMMAND,APP_MESSAGE_SYSTEM_STATUS};
    int64_t deadline=esp_timer_get_time()+500000;
    for (unsigned i=0;i<3;++i) {
        checks[i]=(app_message_t){.type=types[i],.source=APP_ENDPOINT_UART,.target=targets[i],
            .flags=APP_MESSAGE_REQUEST,.generation=app_console_endpoint_generation(APP_ENDPOINT_UART),.deadline_us=deadline};
        snapshots[i]=(app_message_t){0};
    }
    checks[1].payload.command.index=APP_INPUT_SIM_STATUS;
    esp_err_t err=app_console_request_many(checks,snapshots,check_results,3);
    if (err==ESP_OK) for (unsigned i=0;i<3;++i) {
        err=check_results[i]!=ESP_OK ? check_results[i] : snapshots[i].result;
        if (err!=ESP_OK) break;
    }
    if (err==ESP_OK && (snapshots[0].payload.input.sim || snapshots[1].payload.command.flag ||
        snapshots[2].payload.system.mode!=APP_SYSTEM_MODE_NORMAL)) err=ESP_ERR_INVALID_STATE;
    for (unsigned i=0;i<3;++i) app_message_release(&snapshots[i]);
    return err;
}
static void retire(void)
{
    if (token && (uart_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UART) ||
        ui_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI))) token=0;
}
bool display_bench_command(int argc,char **argv)
{
    retire();
    if (argc==3 && !strcmp(argv[0],"atom") && !strcmp(argv[1],"sim") && token) {
        debug_printf("[dbg] ERR SIM transport cannot change during display benchmark\n");return true;
    }
    if (argc!=2 || strcmp(argv[0],"display") || strcmp(argv[1],"bench")) return false;
    if (token) { debug_printf("[dbg] ERR display benchmark pending\n");return true; }
    esp_err_t allowed=preflight();
    if (allowed!=ESP_OK) { debug_printf("[dbg] ERR display bench requires maintenance off, SIM off and idle benchmark (%s)\n",esp_err_to_name(allowed));return true; }
    token=debug_async_token();uncertain=false;uart_epoch=app_console_endpoint_generation(APP_ENDPOINT_UART);
    ui_epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);
    app_message_t message={.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=uart_epoch,.deadline_us=esp_timer_get_time()+500000,
        .payload.command={.token=token,.duration_ms=30000}},reply={0};
    esp_err_t err=app_console_request(&message,&reply);
    if (err==ESP_OK) err=reply.result;
    if (err==ESP_OK && reply.payload.command.token!=token) err=ESP_ERR_INVALID_RESPONSE;
    app_message_release(&reply);
    if (err==ESP_OK) debug_printf("[dbg] OK display bench queued token=%lu duration=30000ms synthetic=1\n",(unsigned long)token);
    else {
        debug_printf("[dbg] ERR display bench %s\n",esp_err_to_name(err));
        /* Admission may already have executed when its transport times out. */
        if (err!=ESP_ERR_TIMEOUT) token=0;
        else { uncertain=true;next_poll=esp_timer_get_time()+200000; }
    }
    return true;
}
void display_bench_event(const app_message_t *message)
{
    retire();
    if (!token || message->type!=APP_MESSAGE_DISPLAY_BENCH || message->source!=APP_ENDPOINT_UI ||
        message->flags!=APP_MESSAGE_EVENT || message->lease || message->generation!=ui_epoch ||
        message->endpoint_epoch!=uart_epoch || message->payload.benchmark.token!=token) return;
    unsigned frames=message->payload.benchmark.frames,bytes=message->payload.benchmark.bytes;
    int64_t elapsed=message->payload.benchmark.elapsed_us;
    debug_printf("[dbg] %s display_bench token=%lu synthetic=1 frames=%u JPEG=%u elapsed_ms=%lu fps=%.2f error=%s\n",
        message->result==ESP_OK && frames==20 ? "DONE" : "FAIL",(unsigned long)token,frames,bytes,
        (unsigned long)(elapsed/1000),elapsed>0 ? (double)frames*1000000/elapsed : 0,esp_err_to_name(message->result));
    token=0;
}
void display_bench_poll(void)
{
    retire();if (!token || !uncertain || esp_timer_get_time()<next_poll) return;
    next_poll=esp_timer_get_time()+200000;
    app_message_t request={.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=uart_epoch,.deadline_us=esp_timer_get_time()+500000,
        .payload.command={.index=APP_UI_BENCH_STATUS,.token=token}},reply={0};
    esp_err_t err=app_console_request(&request,&reply);
    if (err!=ESP_OK) { app_message_release(&reply);return; }
    err=reply.result;
    if (err!=ESP_ERR_NOT_FINISHED) {
        app_message_t event={.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UI,
            .flags=APP_MESSAGE_EVENT,.generation=ui_epoch,.endpoint_epoch=uart_epoch,.payload=reply.payload,
            .result=err==ESP_OK ? reply.payload.benchmark.result : err};
        event.payload.benchmark.token=token;display_bench_event(&event);
    }
    app_message_release(&reply);
}
