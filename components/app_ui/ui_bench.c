#include "ui_mode_messages.h"
#include "ui_bench.h"
#include "app_ui_internal.h"
#include "app_console.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include <stdatomic.h>
#include <string.h>

static atomic_bool busy,ready,cancelled;
static app_message_t job;
static app_message_payload_t completed;
void app_ui_debug_ready(void) { atomic_store(&ready,true); }
static bool valid_job(void)
{
    return !atomic_load(&cancelled) && job.deadline_us>esp_timer_get_time() &&
        job.generation==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
        job.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI);
}
static bool request_cancelled(void *context) { (void)context;return !valid_job(); }
static esp_err_t rpc(app_endpoint_t target,app_message_type_t type,app_message_payload_t payload,
    app_message_t *reply,int64_t deadline)
{
    app_message_t request={.type=type,.source=APP_ENDPOINT_UI,.target=target,
        .flags=APP_MESSAGE_REQUEST,.generation=job.endpoint_epoch,.deadline_us=deadline,.payload=payload};
    bool cleanup=type==APP_MESSAGE_CAMERA_DISPLAY_SESSION && payload.command.index==0;
    esp_err_t err=cleanup ? app_console_request(&request,reply) :
        app_console_request_cancelable(&request,reply,request_cancelled,NULL);
    return err==ESP_OK ? reply->result : err;
}
static void worker(void *unused)
{
    (void)unused;uint8_t *jpeg=NULL;bool reserved=false,drained=false,synthetic=false;
    uint32_t camera_epoch=0;
    app_message_t reply={0},result={.type=APP_MESSAGE_DISPLAY_BENCH,.source=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_EVENT,.generation=job.endpoint_epoch};
    result.payload.benchmark.token=job.payload.command.token;result.result=ESP_ERR_INVALID_STATE;
    if (!valid_job()) goto finished;
    result.result=rpc(APP_ENDPOINT_INPUT,APP_MESSAGE_INPUT_STATUS,(app_message_payload_t){.command={0}},&reply,
        esp_timer_get_time()+500000);
    bool sim=reply.payload.input.sim;app_message_release(&reply);
    if (result.result!=ESP_OK) goto finished;
    if (sim || !valid_job()) { result.result=ESP_ERR_INVALID_STATE;goto finished; }
    result.result=rpc(APP_ENDPOINT_INPUT_SIM,APP_MESSAGE_INPUT_SIM_COMMAND,
        (app_message_payload_t){.command={.index=APP_INPUT_SIM_STATUS}},&reply,esp_timer_get_time()+500000);
    sim=reply.payload.command.flag;app_message_release(&reply);
    if (result.result!=ESP_OK) goto finished;
    if (sim || !valid_job()) { result.result=ESP_ERR_INVALID_STATE;goto finished; }
    result.result=rpc(APP_ENDPOINT_SYSTEM,APP_MESSAGE_SYSTEM_STATUS,(app_message_payload_t){.command={0}},&reply,
        esp_timer_get_time()+500000);
    bool maintenance=reply.payload.system.mode!=APP_SYSTEM_MODE_NORMAL;app_message_release(&reply);
    if (result.result!=ESP_OK) goto finished;
    if (maintenance || !valid_job()) { result.result=ESP_ERR_INVALID_STATE;goto finished; }
    /* Keep the requested reservation token even on transport timeout: acquire
     * may have executed. Cleanup sends release for this exact UI lifetime. */
    reserved=true;
    camera_epoch=app_console_endpoint_generation(APP_ENDPOINT_CAMERA);
    result.result=rpc(APP_ENDPOINT_CAMERA,APP_MESSAGE_CAMERA_DISPLAY_SESSION,
        (app_message_payload_t){.command={.index=1,.token=job.payload.command.token}},&reply,esp_timer_get_time()+3000000);
    drained=result.result==ESP_OK;app_message_release(&reply);
    if (result.result!=ESP_OK) goto finished;
    if (!valid_job()) { result.result=ESP_ERR_INVALID_STATE;goto finished; }
    jpeg=heap_caps_aligned_alloc(16,512*1024,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!jpeg) { result.result=ESP_ERR_NO_MEM;goto finished; }
    size_t length=0;result.result=app_ui_test_jpeg(jpeg,512*1024,&length);
    if (result.result!=ESP_OK) goto finished;
    result.payload.benchmark.bytes=(unsigned)length;
    app_ui_set_sim(true);synthetic=true;
    int64_t start=esp_timer_get_time();
    for (unsigned i=0;i<20;++i) {
        if (!valid_job()) { result.result=ESP_ERR_INVALID_STATE;break; }
        result.result=app_ui_show_jpeg(jpeg,length);
        if (result.result!=ESP_OK) break;
        ++result.payload.benchmark.frames;vTaskDelay(1);
    }
    result.payload.benchmark.elapsed_us=esp_timer_get_time()-start;
finished:
    if (synthetic) app_ui_set_sim(false);
    heap_caps_free(jpeg);
    if (reserved) {
        if (drained && job.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI))
            app_ui_show_connection("Display benchmark finished - waiting for camera...");
        for (;;) {
            if (job.endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI) ||
                camera_epoch!=app_console_endpoint_generation(APP_ENDPOINT_CAMERA)) break;
            esp_err_t err=rpc(APP_ENDPOINT_CAMERA,APP_MESSAGE_CAMERA_DISPLAY_SESSION,
                (app_message_payload_t){.command={.token=job.payload.command.token,.flag=!atomic_load(&cancelled)}},
                &reply,esp_timer_get_time()+500000);
            app_message_release(&reply);
            if (err==ESP_OK || err==ESP_ERR_NOT_FOUND) break;
            if (err!=ESP_ERR_TIMEOUT && result.result==ESP_OK) result.result=err;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    result.payload.benchmark.result=result.result;
    while (!atomic_load(&cancelled) && job.generation==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
        job.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI)) {
        app_message_t delivery=result;
        if (app_console_send(&delivery)==ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    completed=result.payload;
    atomic_store(&busy,false);vTaskDeleteWithCaps(NULL);
}
esp_err_t ui_bench_message(const app_message_t *message,app_message_t *reply)
{
    if (!message || !reply || message->type!=APP_MESSAGE_DISPLAY_BENCH || message->source!=APP_ENDPOINT_UART ||
        message->target!=APP_ENDPOINT_UI || message->flags!=APP_MESSAGE_REQUEST || message->lease ||
        !message->generation || !message->payload.command.token || message->payload.command.index>APP_UI_BENCH_STATUS)
        return ESP_ERR_INVALID_ARG;
    if (message->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (message->generation!=app_console_endpoint_generation(APP_ENDPOINT_UART) ||
        message->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI)) return ESP_ERR_INVALID_STATE;
    if (message->payload.command.index==APP_UI_BENCH_STATUS) {
        if (job.generation!=message->generation || job.payload.command.token!=message->payload.command.token)
            return ESP_ERR_NOT_FOUND;
        if (atomic_load(&busy)) return ESP_ERR_NOT_FINISHED;
        reply->payload=completed;return ESP_OK;
    }
    if (message->payload.command.duration_ms!=30000) return ESP_ERR_INVALID_ARG;
    if (!atomic_load(&ready)) return ESP_ERR_INVALID_STATE;
    if (job.generation==message->generation && job.payload.command.token==message->payload.command.token)
        return ESP_ERR_INVALID_STATE;
    esp_err_t mode=ui_mode_enter_normal(APP_NORMAL_DISPLAY_TEST);
    if(mode!=ESP_OK)return mode;
    bool expected=false;
    if (!atomic_compare_exchange_strong(&busy,&expected,true)) return ESP_ERR_INVALID_STATE;
    job=*message;job.deadline_us=esp_timer_get_time()+30000000;atomic_store(&cancelled,false);
    completed=(app_message_payload_t){.benchmark={.token=message->payload.command.token,.result=ESP_ERR_NOT_FINISHED}};
    if (xTaskCreatePinnedToCoreWithCaps(worker,"display_bench",32768,NULL,4,NULL,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)!=pdPASS) {
        completed.benchmark.result=ESP_ERR_NO_MEM;atomic_store(&busy,false);return ESP_ERR_NO_MEM;
    }
    reply->payload.command.token=message->payload.command.token;return ESP_OK;
}
bool ui_bench_quiesce(uint32_t timeout_ms)
{
    atomic_store(&ready,false);atomic_store(&cancelled,true);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&busy)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}
