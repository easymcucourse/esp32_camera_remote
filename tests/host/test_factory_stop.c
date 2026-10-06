#include "legacy_factory_service.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>

static jmp_buf escape;
static TaskFunction_t worker;
static struct fake_queue { unsigned slot; bool full, alive; } queue;
static bool cooperate, in_worker, close_on_send, close_on_reset;
static unsigned deleted, freezes, resumes, saves, forgets, resets, releases, exits;
static uint32_t counter;
static int64_t now;
static void run(void)
{
    in_worker=true;
    if (!setjmp(escape)) worker(NULL);
    in_worker=false;
}
QueueHandle_t xQueueCreate(unsigned capacity,size_t size)
{
    assert(capacity==1 && size==sizeof(unsigned) && !queue.alive);
    queue.full=false;queue.alive=true;return &queue;
}
void vQueueDelete(QueueHandle_t handle)
{ assert(handle==&queue && queue.alive && !queue.full);queue.alive=false;++deleted; }
BaseType_t xQueueSend(QueueHandle_t handle,const void *item,TickType_t wait)
{
    assert(handle==&queue && queue.alive && !wait);
    unsigned slot=*(const unsigned *)item;
    if (close_on_send && slot<8) {
        close_on_send=false;
        /* Closure while a request sender still owns the queue. The wake fills
         * it; the already reserved request must fail without a dangling token. */
        assert(!app_core_factory_quiesce(0) && queue.alive);
    }
    if (queue.full) return pdFALSE;
    queue.slot=slot;queue.full=true;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t handle,void *item,TickType_t wait)
{
    assert(handle==&queue && queue.alive && (wait==0 || wait==portMAX_DELAY));
    if (!queue.full) {
        if (wait) longjmp(escape,1);
        return pdFALSE;
    }
    *(unsigned *)item=queue.slot;queue.full=false;return pdTRUE;
}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,
    void *context,unsigned priority,void *handle)
{
    assert(!strcmp(name,"factory_all") && stack==4096 && priority==2 && !context && !handle);
    worker=fn;return pdPASS;
}
void vTaskDelay(TickType_t ticks)
{ now+=(int64_t)ticks*1000;if(cooperate && !in_worker)run(); }
void vTaskDelete(void *task) { assert(!task);++exits;longjmp(escape,1); }
int64_t esp_timer_get_time(void) { return now; }
void esp_restart(void) { assert(!"failed/queued jobs must not reboot"); }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "fake"; }
void factory_test_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
static bool freeze(void *ctx,unsigned timeout)
{ assert(ctx==&queue && timeout==5000);++freezes;return true; }
static void resume(void *ctx) { assert(ctx==&queue);++resumes; }
static void snapshot(void *ctx,network_config_t *out)
{ assert(ctx==&queue);network_config_make_default(out);out->channel=11; }
static uint32_t next(void) { return ++counter; }
static bool acquire(void *ctx) { assert(ctx==&queue);return true; }
static void release(void *ctx) { assert(ctx==&queue);++releases; }
static bool save(void *ctx,const network_config_t *cfg)
{ assert(ctx==&queue && cfg);++saves;return true; }
static bool forget(void *ctx) { assert(ctx==&queue);++forgets;return true; }
static bool reset(void *ctx)
{
    assert(ctx==&queue);++resets;
    if(close_on_reset) {
        close_on_reset=false;
        assert(!app_core_factory_quiesce(0) && queue.alive);
        uint32_t ignored=0;assert(app_core_factory_request(&ignored)==ESP_ERR_INVALID_STATE && !ignored);
        assert(app_core_factory_start(NULL)==ESP_ERR_INVALID_STATE);
    }
    return false;
}
int main(void)
{
    app_core_factory_ops_t ops={.transaction={acquire,release,save,forget,reset},
        .freeze_config=freeze,.resume_config=resume,.get_config=snapshot,.next_token=next,.context=&queue};
    uint32_t token=0;esp_err_t result;
    assert(app_core_factory_quiesce(0));
    assert(app_core_factory_request(NULL)==ESP_ERR_INVALID_ARG);
    assert(app_core_factory_start(&ops)==ESP_OK);
    assert(!app_core_factory_quiesce(0) && !deleted && queue.full);
    assert(app_core_factory_request(&token)==ESP_ERR_INVALID_STATE);
    assert(app_core_factory_start(&ops)==ESP_ERR_INVALID_STATE);
    cooperate=true;assert(app_core_factory_quiesce(100)==true && deleted==1 && exits==1);
    assert(app_core_factory_quiesce(0) && deleted==1);

    assert(app_core_factory_start(&ops)==ESP_OK);
    assert(app_core_factory_request(&token)==ESP_OK);
    assert(!app_core_factory_quiesce(0) && deleted==1);
    assert(app_core_factory_quiesce(100));
    assert(app_core_factory_result(token,&result)==ESP_OK && result==ESP_ERR_INVALID_STATE);
    assert(!freezes && !saves && !resets && deleted==2);

    assert(app_core_factory_start(&ops)==ESP_OK);
    close_on_send=true;token=0;
    assert(app_core_factory_request(&token)==ESP_ERR_INVALID_STATE && !token && queue.alive);
    assert(app_core_factory_quiesce(100) && deleted==3 && !freezes);

    assert(app_core_factory_start(&ops)==ESP_OK);
    assert(app_core_factory_request(&token)==ESP_OK);
    close_on_reset=true;run();
    assert(exits==4 && queue.alive && deleted==3);
    assert(freezes==1 && saves==2 && forgets==1 && resets==1 && releases==1 && resumes==1);
    assert(app_core_factory_result(token,&result)==ESP_OK && result==ESP_FAIL);
    assert(app_core_factory_quiesce(0) && deleted==4);
    assert(app_core_factory_request(&token)==ESP_ERR_INVALID_STATE);
    return 0;
}
