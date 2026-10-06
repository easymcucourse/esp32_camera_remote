#include "legacy_factory_service.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>
static jmp_buf escape;
static TaskFunction_t worker;
static struct fake_queue { unsigned slot; bool full; } queue;
static bool fail_queue, fail_task, fail_send, freeze_ok, camera_ok, save_ok = true;
static unsigned freezes, resumes, acquires, saves, forgets, resets, reboots, deleted, snapshots, releases;
static uint32_t token_counter;
QueueHandle_t xQueueCreate(unsigned capacity, size_t size)
{ assert(capacity == 1 && size == sizeof(unsigned)); return fail_queue ? NULL : &queue; }
void vQueueDelete(QueueHandle_t handle) { assert(handle == &queue); ++deleted; }
BaseType_t xQueueSend(QueueHandle_t handle, const void *item, TickType_t wait)
{
    assert(handle == &queue && !wait);
    if (fail_send || queue.full) return pdFALSE;
    queue.slot = *(const unsigned *)item; queue.full = true; return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t handle, void *item, TickType_t wait)
{
    assert(handle == &queue && wait == portMAX_DELAY);
    if (!queue.full) longjmp(escape, 1);
    *(unsigned *)item = queue.slot; queue.full = false; return pdTRUE;
}
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack,
    void *context, unsigned priority, void *handle)
{
    assert(!strcmp(name, "factory_all") && stack == 4096 && priority == 2 && !context && !handle);
    if (fail_task) return pdFALSE;
    worker = fn; return pdPASS;
}
void vTaskDelay(TickType_t ticks) { assert(ticks == 500); }
void vTaskDelete(void *task) { assert(!task); longjmp(escape, 1); }
int64_t esp_timer_get_time(void) { return 0; }
void esp_restart(void) { ++reboots; longjmp(escape, 1); }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "fake"; }
void factory_test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
static void run(void) { if (!setjmp(escape)) worker(NULL); }
static bool freeze(void *context, unsigned timeout)
{ assert(context == &queue && timeout == 5000); ++freezes; return freeze_ok; }
static void resume(void *context) { assert(context == &queue); ++resumes; }
static void snapshot(void *context, network_config_t *config)
{ assert(context == &queue); ++snapshots; network_config_make_default(config); config->channel = 11; }
static uint32_t next_token(void) { return ++token_counter; }
static bool acquire(void *context) { assert(context == &queue); ++acquires; return camera_ok; }
static void release(void *context) { assert(context == &queue); ++releases; }
static bool save(void *context, const network_config_t *config)
{ assert(context == &queue && config); ++saves; return save_ok; }
static bool forget(void *context) { assert(context == &queue); ++forgets; return true; }
static bool reset(void *context) { assert(context == &queue); ++resets; return true; }
int main(void)
{
    app_core_factory_ops_t ops = {.transaction = {acquire, release, save, forget, reset},
        .freeze_config = freeze, .resume_config = resume, .get_config = snapshot,
        .next_token = next_token, .context = &queue};
    uint32_t token; esp_err_t result;
    assert(app_core_factory_request(&token) == ESP_ERR_INVALID_STATE);
    assert(app_core_factory_start(NULL) == ESP_ERR_INVALID_ARG);
    fail_queue = true; assert(app_core_factory_start(&ops) == ESP_ERR_NO_MEM); fail_queue = false;
    fail_task = true; assert(app_core_factory_start(&ops) == ESP_ERR_NO_MEM && deleted == 1); fail_task = false;
    assert(app_core_factory_start(&ops) == ESP_OK);
    assert(app_core_factory_start(&ops) == ESP_ERR_INVALID_STATE);
    fail_send = true; assert(app_core_factory_request(&token) == ESP_ERR_INVALID_STATE); fail_send = false;
    assert(app_core_factory_request(&token) == ESP_OK);
    uint32_t first = token;
    assert(app_core_factory_request(&token) == ESP_ERR_INVALID_STATE);
    assert(app_core_factory_result(first, &result) == ESP_ERR_NOT_FINISHED);
    run(); assert(app_core_factory_result(first, &result) == ESP_OK && result == ESP_ERR_TIMEOUT);
    assert(!snapshots && !acquires && !resumes && !reboots);
    freeze_ok = true;
    assert(app_core_factory_request(&token) == ESP_OK); run();
    assert(app_core_factory_result(token, &result) == ESP_OK && result == ESP_ERR_TIMEOUT && resumes == 1);
    camera_ok = true; save_ok = false;
    assert(app_core_factory_request(&token) == ESP_OK); run();
    assert(app_core_factory_result(token, &result) == ESP_OK && result == ESP_FAIL && resumes == 2 && releases == 1 && !reboots);
    for (unsigned i = 0; i < 8; ++i) { assert(app_core_factory_request(&token) == ESP_OK); run(); }
    assert(app_core_factory_result(first, &result) == ESP_ERR_NOT_FOUND);
    save_ok = true;
    assert(app_core_factory_request(&token) == ESP_OK); run();
    assert(app_core_factory_result(token, &result) == ESP_OK && result == ESP_OK && reboots == 1 && forgets == 1 && resets == 1);
    assert(app_core_factory_request(&token) == ESP_ERR_INVALID_STATE);
    assert(app_core_factory_result(0, &result) == ESP_ERR_INVALID_ARG);
    return 0;
}
