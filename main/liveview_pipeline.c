#include "liveview_pipeline.h"
#include "ptpip_packet.h"
#include "board_7b.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
static const char *TAG = "camera_pair";

static bool display_object(const uint8_t *data, size_t size, size_t *jpeg_size)
{
    size_t offset = size >= 4 ? get32(data) : size;
    if (offset > size || size - offset < 3 || data[offset] != 0xff || data[offset + 1] != 0xd8 || data[offset + 2] != 0xff) {
        ESP_LOGE(TAG, "Live-view JPEG header mismatch: object=%u offset=%u", (unsigned)size, (unsigned)offset);
        return false;
    }
    size_t end = offset + 2;
    while (end + 1 < size && !(data[end] == 0xff && data[end + 1] == 0xd9)) ++end;
    if (end + 1 >= size) {
        ESP_LOGE(TAG, "Live-view JPEG missing EOI");
        return false;
    }
    *jpeg_size = end + 2 - offset;
    return board_7b_show_jpeg(data + offset, *jpeg_size) == ESP_OK;
}

void jpeg_decode_task(void *arg)
{
    jpeg_pipeline_t *pipeline = arg;
    unsigned frames = 0, window_frames = 0;
    int64_t window_start = esp_timer_get_time();
    ESP_LOGI(TAG, "JPEG worker: core=%d, ESP32-S3 SIMD RGB565 decoder", xPortGetCoreID());
    for (;;) {
        jpeg_job_t job;
        xQueueReceive(pipeline->ready, &job, portMAX_DELAY);
        if (job.slot < 0) break;
        if (!atomic_load(&pipeline->failed)) {
            int64_t start = esp_timer_get_time();
            size_t jpeg_size = 0;
            if (!display_object(pipeline->data[job.slot], job.size, &jpeg_size)) {
                atomic_store(&pipeline->failed, true);
            } else {
                int64_t shown = esp_timer_get_time();
                ++frames;
                ++window_frames;
                if (frames == 1 || shown - window_start >= 5000000) {
                    ESP_LOGI(TAG, "LIVEVIEW frames=%u fps=%.2f JPEG=%u read=%ldms display=%ldms stack_free=%u",
                             frames, (double)window_frames * 1000000 / (shown - window_start),
                             (unsigned)jpeg_size, (long)job.read_ms, (long)((shown - start) / 1000),
                             (unsigned)uxTaskGetStackHighWaterMark(NULL));
                    window_start = shown;
                    window_frames = 0;
                }
            }
        }
        // Ownership returns only after decode and LCD publication have finished.
        xQueueSend(pipeline->free_slots, &job.slot, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "JPEG worker drained: displayed=%u", frames);
    // No pipeline access after this signal: the producer may free its context.
    xSemaphoreGive(pipeline->done);
    vTaskDeleteWithCaps(NULL);
}

