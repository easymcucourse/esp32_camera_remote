#include "liveview_pipeline.h"
#include "board_7b.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static jpeg_job_t jobs[16];
static unsigned read_job, returned, shown, recoveries, deleted, done;
static esp_err_t display_results[16], recovery_result;
static uint8_t valid[] = {4,0,0,0, 0xff,0xd8,0xff,0xda,0,2,0xff,0xd9};
static uint8_t broken[] = {4,0,0,0, 0,0,0,0};
static int64_t clock_ms;
void fake_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
const char *esp_err_to_name(esp_err_t err) { (void)err; return "fake"; }
int64_t esp_timer_get_time(void) { return ++clock_ms * 1000; }
int xPortGetCoreID(void) { return 1; }
unsigned uxTaskGetStackHighWaterMark(void *task) { assert(!task); return 4096; }
void vTaskDeleteWithCaps(void *task) { assert(!task && done == 1); ++deleted; }
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout)
{ assert(queue == jobs && timeout == portMAX_DELAY && read_job < 16); *(jpeg_job_t *)item = jobs[read_job++]; return pdTRUE; }
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout)
{ assert(queue == &returned && timeout == portMAX_DELAY); assert(*(const int *)item >= 0 && !done); ++returned; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { assert(sem == &done && !done); ++done; return pdTRUE; }
esp_err_t board_7b_show_jpeg(const uint8_t *jpeg, size_t length)
{ assert(jpeg == valid + 4 && length == sizeof(valid) - 4 && !done); return display_results[shown++]; }
esp_err_t board_7b_recover_display(void) { ++recoveries; assert(!done); return recovery_result; }

static bool run(bool malformed_first, unsigned count)
{
    read_job = returned = shown = recoveries = deleted = done = 0;
    jpeg_pipeline_t pipeline = {.data = {broken, valid}, .ready = jobs, .free_slots = &returned, .done = &done};
    atomic_init(&pipeline.failed, false);
    for (unsigned i = 0; i < count; ++i)
        jobs[i] = (jpeg_job_t){.slot = malformed_first && i == 0 ? 0 : 1,
            .size = malformed_first && i == 0 ? sizeof(broken) : sizeof(valid)};
    jobs[count] = (jpeg_job_t){.slot = -1};
    jpeg_decode_task(&pipeline);
    assert(returned == count && read_job == count + 1 && done == 1 && deleted == 1);
    return atomic_load(&pipeline.failed);
}
int main(void)
{
    display_results[0] = ESP_ERR_INVALID_RESPONSE; display_results[1] = ESP_OK;
    assert(!run(true, 3) && shown == 2 && recoveries == 0);
    display_results[0] = ESP_ERR_INVALID_STATE; display_results[1] = ESP_OK; recovery_result = ESP_OK;
    assert(!run(false, 2) && shown == 2 && recoveries == 1);
    recovery_result = ESP_FAIL;
    assert(run(false, 3) && shown == 1 && recoveries == 1); /* Queued buffers still return after fatal recovery. */
    display_results[0] = ESP_ERR_NO_MEM;
    assert(run(false, 3) && shown == 1 && recoveries == 0);
    memset(display_results, 0, sizeof(display_results));
    assert(!run(false, 2) && shown == 2);
    assert(!run(false, 0) && shown == 0); /* Stop before the first frame. */
    for (unsigned i = 0; i < 16; ++i) display_results[i] = ESP_ERR_INVALID_RESPONSE;
    assert(run(false, 12) && shown == 10 && recoveries == 0);
    display_results[5] = ESP_OK;
    assert(!run(false, 12) && shown == 12); /* A good frame resets the damage streak. */
    puts("Liveview damaged frame continuation, recovery, fatal drain and slot ownership passed");
}
