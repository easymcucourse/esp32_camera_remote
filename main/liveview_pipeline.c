#include "liveview_pipeline.h"
#include "sony_liveview.h"
#include "board_7b.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
static const char *TAG = "camera_pair";

void jpeg_decode_task(void *arg)
{
    jpeg_pipeline_t *pipeline = arg;
    unsigned frames = 0, window_frames = 0, dropped = 0;
    unsigned consecutive_bad = 0;
    int64_t window_start = esp_timer_get_time();
    int64_t last_drop_log = window_start;
    ESP_LOGI(TAG, "JPEG worker: core=%d, ESP32-S3 SIMD RGB565 decoder", xPortGetCoreID());
    for (;;) {
        jpeg_job_t job;
        xQueueReceive(pipeline->ready, &job, portMAX_DELAY);
        if (job.slot < 0) break;
        if (!atomic_load(&pipeline->failed)) {
            int64_t start = esp_timer_get_time();
            sony_liveview_t view;
            esp_err_t result = sony_liveview_parse(pipeline->data[job.slot], job.size, &view) ?
                board_7b_show_jpeg(view.jpeg, view.jpeg_size) : ESP_ERR_INVALID_RESPONSE;
            if (result == ESP_ERR_INVALID_RESPONSE) {
                ++dropped;
                if (dropped == 1 || start - last_drop_log >= 5000000) {
                    ESP_LOGW(TAG, "LIVEVIEW damaged frames dropped=%u; keeping session and last image", dropped);
                    last_drop_log = start;
                }
                if (++consecutive_bad >= 10) {
                    ESP_LOGE(TAG, "LIVEVIEW ten consecutive damaged frames; reconnecting camera");
                    atomic_store(&pipeline->failed, true);
                }
            } else if (result == ESP_ERR_INVALID_STATE) {
                /* Recovery retires the uncertain framebuffer. Drop this image;
                 * the next job decodes into the newly acquired back buffer. */
                if (board_7b_recover_display() != ESP_OK) atomic_store(&pipeline->failed, true);
            } else if (result != ESP_OK) {
                ESP_LOGE(TAG, "LIVEVIEW display failed: %s", esp_err_to_name(result));
                atomic_store(&pipeline->failed, true);
            } else {
                consecutive_bad = 0;
                int64_t shown = esp_timer_get_time();
                ++frames;
                ++window_frames;
                if (frames == 1 || shown - window_start >= 5000000) {
                    ESP_LOGI(TAG, "LIVEVIEW frames=%u fps=%.2f JPEG=%u read=%ldms display=%ldms stack_free=%u",
                             frames, (double)window_frames * 1000000 / (shown - window_start),
                             (unsigned)view.jpeg_size, (long)job.read_ms, (long)((shown - start) / 1000),
                             (unsigned)uxTaskGetStackHighWaterMark(NULL));
                    window_start = shown;
                    window_frames = 0;
                }
            }
        }
        // Ownership returns only after decode and LCD publication have finished.
        xQueueSend(pipeline->free_slots, &job.slot, portMAX_DELAY);
    }
    ESP_LOGI(TAG, "JPEG worker drained: displayed=%u dropped=%u", frames, dropped);
    // No pipeline access after this signal: the producer may free its context.
    xSemaphoreGive(pipeline->done);
    vTaskDeleteWithCaps(NULL);
}

