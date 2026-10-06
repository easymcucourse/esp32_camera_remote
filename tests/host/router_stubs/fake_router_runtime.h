#pragma once
/* Cooperative host scheduler only: no real FreeRTOS threads or SMP proof. */
#include "app_console.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

struct fake_semaphore { unsigned count; bool mutex; };
struct fake_queue { unsigned capacity, count, front; size_t item_size; unsigned char *data; };
static int64_t now_us = 1000;
static unsigned tasks;
static unsigned mutex_busy_ms;
static void (*on_wait)(void);
static void (*on_unlock)(void);
static TaskFunction_t housekeeping;
static void *housekeeping_context;
static bool scheduling, hold_housekeeping;
static unsigned task_exits;
static jmp_buf task_yield;
static void run_housekeeping(void)
{
    if (!housekeeping || scheduling || hold_housekeeping) return;
    scheduling = true;
    if (!setjmp(task_yield)) housekeeping(housekeeping_context);
    scheduling = false;
}
int64_t esp_timer_get_time(void) { return now_us; }
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{ SemaphoreHandle_t p = calloc(1, sizeof(*p)); assert(p); p->count = 1; p->mutex = true; return p; }
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{ SemaphoreHandle_t p = calloc(1, sizeof(*p)); assert(p); return p; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t semaphore, TickType_t timeout)
{
    assert(semaphore);
    if (semaphore->mutex && mutex_busy_ms) {
        unsigned busy=mutex_busy_ms;mutex_busy_ms=0;
        if (!timeout) return pdFALSE;
        unsigned waited=timeout<busy ? timeout : busy;
        now_us+=(int64_t)waited*1000;
        if (waited<busy) return pdFALSE;
    }
    if (!semaphore->count && timeout) {
        assert(!semaphore->mutex); /* Destructor/callback reentry must be outside router locks. */
        void (*work)(void) = on_wait; on_wait = NULL;
        if (work) work();
        if (!semaphore->count) now_us += (int64_t)timeout * 1000;
    }
    if (!semaphore->count) return pdFALSE;
    --semaphore->count; return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t semaphore)
{
    assert(semaphore); semaphore->count = 1;
    void (*work)(void)=semaphore->mutex ? on_unlock : NULL;if(work){on_unlock=NULL;work();}
    /* A newly stopped worker may run as soon as the router unlocks. Retain
     * normal fake scheduling (manual poll) until status.running is false. */
    if (semaphore->mutex && housekeeping && !scheduling && !hold_housekeeping) {
        scheduling=true;
        app_console_status_t state; app_console_get_status(&state);
        scheduling=false;
        if (!state.running) run_housekeeping();
    }
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t semaphore) { free(semaphore); }
QueueHandle_t xQueueCreate(unsigned capacity, size_t size)
{
    QueueHandle_t q = calloc(1, sizeof(*q)); assert(q);
    q->data = calloc(capacity, size); assert(q->data);
    q->capacity = capacity; q->item_size = size; return q;
}
void vQueueDelete(QueueHandle_t q) { free(q->data); free(q); }
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t timeout)
{
    assert(q && !timeout);
    if (q->count == q->capacity) return pdFALSE;
    memcpy(q->data + ((q->front + q->count) % q->capacity) * q->item_size, item, q->item_size);
    ++q->count; return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t timeout)
{
    assert(q && !timeout);
    if (!q->count) return pdFALSE;
    memcpy(item, q->data + q->front * q->item_size, q->item_size);
    --q->count; q->front = (q->front + 1) % q->capacity; return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { return q->count; }
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack, void *context, unsigned priority, void *handle)
{ assert(fn && context && !strcmp(name, "message_router") && stack == 4096 && priority == 5 && !handle); housekeeping=fn;housekeeping_context=context;++tasks; return pdPASS; }
void vTaskDelay(TickType_t delay)
{ now_us += (int64_t)delay * 1000;if(scheduling)longjmp(task_yield,1);run_housekeeping(); }
void vTaskDelete(void *task)
{ assert(!task && scheduling);housekeeping=NULL;++task_exits;longjmp(task_yield,1); }
