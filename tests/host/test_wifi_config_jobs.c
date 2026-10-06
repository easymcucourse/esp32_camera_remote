#include "wifi_config_jobs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

struct fake_queue { unsigned count; size_t size; uint8_t data[2][256]; };
struct fake_semaphore { bool free; };
static bool fail_queue, fail_lock, fail_task, step_delay, in_worker;
static TaskFunction_t worker;
static void *worker_context;
static jmp_buf escape;
static int64_t now;
static unsigned saves, restarts;
static app_wifi_result_t save_error;
static unsigned fail_restarts;
static network_config_t durable;
static wifi_config_jobs_t *close_during_save;
static void run(void) { in_worker = true; if (!setjmp(escape)) worker(worker_context); in_worker = false; }
QueueHandle_t xQueueCreate(unsigned capacity, size_t size)
{
    assert(capacity == 2 && size <= 256); if (fail_queue) return NULL;
    struct fake_queue *q = calloc(1, sizeof(*q)); q->size = size; return q;
}
void vQueueDelete(QueueHandle_t q) { free(q); }
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait)
{
    assert(!wait); if (q->count == 2) return pdFALSE;
    memcpy(q->data[q->count++], item, q->size); return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait)
{
    if (!q->count) { if (wait) longjmp(escape, 1); return pdFALSE; }
    memcpy(item, q->data[0], q->size); --q->count; memcpy(q->data[0], q->data[1], q->size); return pdTRUE;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    if (fail_lock) return NULL;
    struct fake_semaphore *s = malloc(sizeof(*s)); s->free = true; return s;
}
void vSemaphoreDelete(SemaphoreHandle_t s) { free(s); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait)
{ (void)wait; if (!s->free) return pdFALSE; s->free = false; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(!s->free); s->free = true; return pdTRUE; }
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack,
    void *context, unsigned priority, void *handle)
{
    assert(!strcmp(name, "wifi_config") && stack == 4096 && priority == 2 && !handle);
    if (fail_task) return pdFALSE;
    worker = fn; worker_context = context; return pdPASS;
}
void vTaskDelete(void *task) { assert(!task); longjmp(escape, 1); }
void vTaskDelay(TickType_t ticks) { now += (int64_t)ticks * 1000; if (step_delay && !in_worker) run(); }
int64_t esp_timer_get_time(void) { return now; }
static app_wifi_result_t save(void *context, const network_config_t *config)
{
    assert(context == &durable); ++saves;
    if (close_during_save) {
        wifi_config_jobs_t *active = close_during_save; close_during_save = NULL;
        assert(wifi_jobs_stop(active, 0) == APP_WIFI_TIMEOUT);
        uint32_t ignored;
        assert(wifi_jobs_apply(active, config, false, &ignored) == APP_WIFI_STATE);
    }
    if (save_error != APP_WIFI_OK) return save_error;
    durable = *config; return APP_WIFI_OK;
}
static app_wifi_result_t restart(void *context, const network_config_t *config)
{ assert(context == &durable && config); ++restarts; if (fail_restarts) { --fail_restarts; return APP_WIFI_IO; } return APP_WIFI_OK; }
int main(void)
{
    network_config_make_default(&durable);
    fail_queue = true; assert(!wifi_jobs_create(&durable, save, restart)); fail_queue = false;
    fail_lock = true; assert(!wifi_jobs_create(&durable, save, restart)); fail_lock = false;
    wifi_config_jobs_t *j = wifi_jobs_create(&durable, save, restart); assert(j);
    fail_task = true; assert(wifi_jobs_start(j) == APP_WIFI_NO_MEMORY); fail_task = false;
    assert(wifi_jobs_start(j) == APP_WIFI_OK && wifi_jobs_start(j) == APP_WIFI_STATE);
    network_config_t config = durable, snapshot;
    strcpy(config.ssid, "job-host"); uint32_t token, token2, ignored; app_wifi_result_t result;
    assert(wifi_jobs_apply(j, &config, true, &token) == APP_WIFI_OK);
    assert(wifi_jobs_get(j, &snapshot) == APP_WIFI_OK && strcmp(snapshot.ssid, "job-host"));
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_PENDING && !saves && !restarts);
    assert(wifi_jobs_cancel(j, token) == APP_WIFI_OK); run();
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_CANCELLED && !saves);
    assert(wifi_jobs_apply(j, &config, true, &token) == APP_WIFI_OK);
    assert(wifi_jobs_commit(j, token, 10001) == APP_WIFI_INVALID);
    assert(wifi_jobs_commit(j, token, 500) == APP_WIFI_OK);
    assert(wifi_jobs_commit(j, token, 0) == APP_WIFI_STATE && wifi_jobs_cancel(j, token) == APP_WIFI_STATE);
    run(); assert(now >= 500000 && wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_OK);
    assert(wifi_jobs_get(j, &snapshot) == APP_WIFI_OK && !strcmp(snapshot.ssid, "job-host") && saves == 1 && restarts == 1);
    assert(wifi_jobs_apply(j, &config, true, &token) == APP_WIFI_OK); run();
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_TIMEOUT && saves == 1);
    assert(wifi_jobs_apply(j, &config, true, &token) == APP_WIFI_OK);
    assert(wifi_jobs_apply(j, &config, true, &token2) == APP_WIFI_OK);
    assert(wifi_jobs_apply(j, &config, false, &ignored) == APP_WIFI_STATE);
    assert(wifi_jobs_freeze(j, 0) == APP_WIFI_TIMEOUT);
    assert(wifi_jobs_cancel(j, token) == APP_WIFI_OK && wifi_jobs_cancel(j, token2) == APP_WIFI_OK); run();
    assert(wifi_jobs_freeze(j, 0) == APP_WIFI_OK);
    assert(wifi_jobs_apply(j, &config, false, &ignored) == APP_WIFI_STATE && wifi_jobs_freeze(j, 0) == APP_WIFI_STATE);
    wifi_jobs_resume(j); config.channel = 7; fail_restarts = 1;
    assert(wifi_jobs_apply(j, &config, false, &token) == APP_WIFI_OK); run();
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_IO);
    assert(durable.channel == 6 && wifi_jobs_get(j, &snapshot) == APP_WIFI_OK && snapshot.channel == 6);
    save_error = APP_WIFI_TIMEOUT;
    assert(wifi_jobs_apply(j, &config, false, &token) == APP_WIFI_OK); run();
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_TIMEOUT); save_error = APP_WIFI_OK;
    uint32_t old = token;
    for (unsigned i = 0; i < 9; ++i) { assert(wifi_jobs_apply(j, &config, false, &token) == APP_WIFI_OK); run(); }
    assert(wifi_jobs_result(j, old, &result) == APP_WIFI_NOT_FOUND);
    assert(wifi_jobs_apply(j, &config, true, &token) == APP_WIFI_OK);
    assert(wifi_jobs_stop(j, 0) == APP_WIFI_TIMEOUT);
    assert(wifi_jobs_apply(j, &config, false, &ignored) == APP_WIFI_STATE);
    step_delay = true; assert(wifi_jobs_stop(j, 1000) == APP_WIFI_OK);
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_CANCELLED);
    assert(wifi_jobs_start(j) == APP_WIFI_OK && wifi_jobs_stop(j, 1000) == APP_WIFI_OK);
    wifi_jobs_destroy(j);
    /* Closure inside a persistence call keeps that physical apply alive until
     * restart/commit completes; only then can the worker acknowledge stop. */
    j = wifi_jobs_create(&durable, save, restart); assert(j);
    assert(wifi_jobs_start(j) == APP_WIFI_OK);
    wifi_jobs_set(j, &durable, 13); config = durable; config.channel = 9;
    unsigned saves_before = saves, restarts_before = restarts;
    assert(wifi_jobs_apply(j, &config, false, &token) == APP_WIFI_OK);
    close_during_save = j; run();
    assert(!close_during_save && wifi_jobs_stop(j, 0) == APP_WIFI_OK);
    assert(wifi_jobs_result(j, token, &result) == APP_WIFI_OK && result == APP_WIFI_OK);
    assert(saves == saves_before + 1 && restarts == restarts_before + 1 && durable.channel == 9);
    assert(wifi_jobs_get(j, &snapshot) == APP_WIFI_OK && snapshot.channel == 9);
    assert(wifi_jobs_apply(j, &config, false, &ignored) == APP_WIFI_STATE);
    wifi_jobs_resume(j); assert(wifi_jobs_apply(j, &config, false, &ignored) == APP_WIFI_STATE);
    uint32_t normal_token=token;
    fail_task=true;assert(wifi_jobs_start(j)==APP_WIFI_NO_MEMORY);fail_task=false;
    assert(wifi_jobs_apply(j,&config,false,&ignored)==APP_WIFI_STATE);
    assert(wifi_jobs_start(j)==APP_WIFI_OK && wifi_jobs_start(j)==APP_WIFI_STATE);
    assert(wifi_jobs_result(j,normal_token,&result)==APP_WIFI_OK && result==APP_WIFI_OK);
    config.channel=10;assert(wifi_jobs_apply(j,&config,true,&token2)==APP_WIFI_OK);
    assert(wifi_jobs_commit(j,token2,1500)==APP_WIFI_OK);
    run();assert(wifi_jobs_result(j,token2,&result)==APP_WIFI_OK && result==APP_WIFI_OK);
    assert(wifi_jobs_get(j,&snapshot)==APP_WIFI_OK && snapshot.channel==10);
    assert(wifi_jobs_result(j,normal_token,&result)==APP_WIFI_OK && result==APP_WIFI_OK);
    step_delay=true;assert(wifi_jobs_stop(j,1000)==APP_WIFI_OK);
    wifi_jobs_destroy(j); return 0;
}
