#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../../components/app_ui/ui_bench.c"
static uint32_t uart_epoch=2,ui_epoch=3,camera_epoch=4;
static int64_t clock_us=1000;
static bool create_fail,alloc_fail,sim_enabled,input_sim,lease_held,retire_uart;
static bool maintenance;
static unsigned renders,freed,events,acquires,releases,restores,deleted,blocked_events;
static esp_err_t acquire_error,render_error;
static TaskFunction_t task;
static app_message_t completion;
int64_t esp_timer_get_time(void) { return clock_us; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ return endpoint==APP_ENDPOINT_UART ? uart_epoch : endpoint==APP_ENDPOINT_UI ? ui_epoch : camera_epoch; }
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn,const char *name,unsigned stack,void *context,
    unsigned priority,void *handle,unsigned core,unsigned caps)
{ assert(!strcmp(name,"display_bench") && stack==32768 && priority==4 && core==1 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT) && !context && !handle);task=fn;return create_fail ? pdFALSE : pdPASS; }
void vTaskDeleteWithCaps(void *handle) { assert(!handle);++deleted; }
void vTaskDelay(TickType_t ticks) { clock_us+=(int64_t)ticks*1000;if(retire_uart) { ++uart_epoch;retire_uart=false; } }
void *heap_caps_aligned_alloc(size_t alignment,size_t size,unsigned caps)
{ assert(lease_held && alignment==16 && size==512*1024 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));return alloc_fail ? NULL : malloc(size); }
void heap_caps_free(void *pointer) { if(pointer){++freed;free(pointer);} }
esp_err_t app_ui_test_jpeg(uint8_t *output,size_t capacity,size_t *length)
{ assert(output && capacity==512*1024 && lease_held);*length=4096;return ESP_OK; }
esp_err_t app_ui_show_jpeg(const uint8_t *data,size_t length)
{ assert(data && length==4096 && lease_held);++renders;return render_error; }
void app_ui_set_sim(bool enabled) { assert(lease_held);(void)enabled; }
esp_err_t app_ui_show_connection(const char *text) { assert(lease_held && strstr(text,"benchmark"));++restores;return ESP_OK; }
void app_message_release(app_message_t *message) { assert(!message->lease);memset(message,0,sizeof(*message)); }
esp_err_t app_console_request(app_message_t *request,app_message_t *reply)
{
    assert(request->source==APP_ENDPOINT_UI && request->generation==ui_epoch && request->deadline_us>clock_us);
    *reply=(app_message_t){.result=ESP_OK};
    switch(request->type) {
    case APP_MESSAGE_INPUT_STATUS:reply->payload.input.sim=input_sim;break;
    case APP_MESSAGE_INPUT_SIM_COMMAND:assert(request->payload.command.index==APP_INPUT_SIM_STATUS);reply->payload.command.flag=sim_enabled;break;
    case APP_MESSAGE_SYSTEM_STATUS:reply->payload.system.mode=maintenance ? APP_SYSTEM_MODE_MAINT : APP_SYSTEM_MODE_NORMAL;break;
    case APP_MESSAGE_CAMERA_DISPLAY_SESSION:
        assert(request->payload.command.token==job.payload.command.token);
        if(request->payload.command.index) { ++acquires;lease_held=true;if(acquire_error)return acquire_error;reply->payload.command.flag=true; }
        else { assert(lease_held);++releases;lease_held=false; }
        break;
    default:assert(false);
    }
    return ESP_OK;
}
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->type==APP_MESSAGE_DISPLAY_BENCH && message->flags==APP_MESSAGE_EVENT && message->generation==ui_epoch && !lease_held);
    if(blocked_events){--blocked_events;return ESP_ERR_TIMEOUT;}++events;completion=*message;return ESP_OK;
}
esp_err_t app_console_request_cancelable(app_message_t *message,app_message_t *reply,
    app_console_cancel_fn cancelled_request,void *context)
{
    assert(cancelled_request && !context);
    if(cancelled_request(context)) return ESP_ERR_INVALID_STATE;
    return app_console_request(message,reply);
}
static app_message_t request(uint32_t token)
{ return (app_message_t){.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
    .flags=APP_MESSAGE_REQUEST,.generation=uart_epoch,.endpoint_epoch=ui_epoch,.deadline_us=clock_us+500000,
    .payload.command={.token=token,.duration_ms=30000}}; }
static void run(void) { assert(task);task(NULL);assert(!atomic_load(&busy) && !lease_held); }
int main(void)
{
    app_message_t m=request(1),reply={0};assert(ui_bench_message(&m,&reply)==ESP_ERR_INVALID_STATE);
    app_ui_debug_ready();create_fail=true;assert(ui_bench_message(&m,&reply)==ESP_ERR_NO_MEM && !atomic_load(&busy));create_fail=false;
    m=request(2);assert(ui_bench_message(&m,&reply)==ESP_OK && reply.payload.command.token==2);
    assert(ui_bench_message(&m,&reply)==ESP_ERR_INVALID_STATE);
    app_message_t query=m;query.payload.command.index=APP_UI_BENCH_STATUS;
    assert(ui_bench_message(&query,&reply)==ESP_ERR_NOT_FINISHED);
    blocked_events=2;run();assert(renders==20 && freed==1 && events==1 && acquires==1 && releases==1 && restores==1 && deleted==1);
    assert(completion.result==ESP_OK && completion.payload.benchmark.frames==20 && completion.payload.benchmark.bytes==4096);
    assert(ui_bench_message(&query,&reply)==ESP_OK && reply.payload.benchmark.token==2 && reply.payload.benchmark.frames==20);
    assert(ui_bench_message(&m,&reply)==ESP_ERR_INVALID_STATE);
    sim_enabled=true;m=request(3);assert(ui_bench_message(&m,&reply)==ESP_OK);run();assert(completion.result==ESP_ERR_INVALID_STATE && acquires==1);sim_enabled=false;
    maintenance=true;m=request(90);assert(ui_bench_message(&m,&reply)==ESP_OK);run();assert(completion.result==ESP_ERR_INVALID_STATE && acquires==1);maintenance=false;
    acquire_error=ESP_ERR_TIMEOUT;m=request(4);assert(ui_bench_message(&m,&reply)==ESP_OK);run();assert(releases==2 && completion.result==ESP_ERR_TIMEOUT && restores==1);acquire_error=ESP_OK;
    alloc_fail=true;m=request(5);assert(ui_bench_message(&m,&reply)==ESP_OK);run();assert(completion.result==ESP_ERR_NO_MEM && releases==3);alloc_fail=false;
    render_error=ESP_ERR_INVALID_RESPONSE;m=request(6);assert(ui_bench_message(&m,&reply)==ESP_OK);run();assert(completion.result==render_error && releases==4);render_error=ESP_OK;
    m=request(7);assert(ui_bench_message(&m,&reply)==ESP_OK);assert(!ui_bench_quiesce(0));unsigned old=events;run();assert(events==old && ui_bench_quiesce(0));
    app_ui_debug_ready();m=request(8);assert(ui_bench_message(&m,&reply)==ESP_OK);retire_uart=true;run();assert(events==old && releases==5);
    m=request(9);m.payload.command.duration_ms=1;assert(ui_bench_message(&m,&reply)==ESP_ERR_INVALID_ARG);
    m=request(9);--m.generation;assert(ui_bench_message(&m,&reply)==ESP_ERR_INVALID_STATE);
    m=request(9);m.deadline_us=clock_us;assert(ui_bench_message(&m,&reply)==ESP_ERR_TIMEOUT);
    puts("UI benchmark resource ownership, typed reservation, cancellation and completion pressure passed");return 0;
}

esp_err_t ui_mode_enter_normal(unsigned reason) { assert(reason==APP_NORMAL_DISPLAY_TEST);return ESP_OK; }
