#include "legacy_factory_service.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdatomic.h>

static app_core_factory_ops_t operations;
static QueueHandle_t requests;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static bool pending;
static atomic_bool closing = true, running;
static atomic_uint admissions;
static unsigned next_slot;
static struct { uint32_t token; esp_err_t result; bool complete; } results[8];
static void task(void *unused)
{
    (void)unused;
    for (;;) {
        unsigned slot;
        if (xQueueReceive(requests, &slot, portMAX_DELAY) != pdTRUE) continue;
        if (slot >= 8) {
            if (atomic_load(&closing)) break;
            continue;
        }
        esp_err_t result = atomic_load(&closing) ? ESP_ERR_INVALID_STATE : ESP_ERR_TIMEOUT;
        bool frozen = !atomic_load(&closing) && operations.freeze_config(operations.context, 5000);
        if (frozen) {
            network_config_t current; operations.get_config(operations.context, &current);
            factory_reset_result_t reset = factory_reset_all(&current, &operations.transaction, operations.context);
            result = reset == FACTORY_RESET_OK ? ESP_OK : reset == FACTORY_RESET_BUSY ? ESP_ERR_TIMEOUT : ESP_FAIL;
            if (result != ESP_OK) operations.resume_config(operations.context);
        }
        portENTER_CRITICAL(&mux);
        results[slot].result = result; results[slot].complete = true;
        if (result != ESP_OK) pending = false;
        uint32_t token = results[slot].token;
        portEXIT_CRITICAL(&mux);
        ESP_LOGI("factory_reset", "Factory all token=%lu result=%s; Wi-Fi/camera/UI persistence targeted",
            (unsigned long)token, esp_err_to_name(result));
        if (result == ESP_OK) { vTaskDelay(pdMS_TO_TICKS(500)); esp_restart(); }
        if (atomic_load(&closing)) break;
    }
    /* An admitted sender may still own the queue after reserving its slot. */
    while (atomic_load(&admissions)) vTaskDelay(pdMS_TO_TICKS(10));
    unsigned slot;
    while (xQueueReceive(requests, &slot, 0) == pdTRUE) if (slot < 8) {
        portENTER_CRITICAL(&mux);
        results[slot].result = ESP_ERR_INVALID_STATE; results[slot].complete = true;
        pending = false;
        portEXIT_CRITICAL(&mux);
    }
    atomic_store(&running, false);
    vTaskDelete(NULL);
}
esp_err_t app_core_factory_start(const app_core_factory_ops_t *ops)
{
    if (requests) return ESP_ERR_INVALID_STATE;
    if (!ops || !ops->freeze_config || !ops->resume_config || !ops->get_config || !ops->next_token ||
        !ops->transaction.acquire_camera || !ops->transaction.release_camera || !ops->transaction.save_wifi ||
        !ops->transaction.forget_camera || !ops->transaction.reset_ui) return ESP_ERR_INVALID_ARG;
    operations = *ops;
    requests = xQueueCreate(1, sizeof(unsigned));
    if (!requests) return ESP_ERR_NO_MEM;
    atomic_store(&running, true);
    if (xTaskCreate(task, "factory_all", 4096, NULL, 2, NULL) != pdPASS) {
        atomic_store(&closing, true); atomic_store(&running, false);
        vQueueDelete(requests); requests = NULL; return ESP_ERR_NO_MEM;
    }
    atomic_store(&closing, false);
    return ESP_OK;
}
static esp_err_t request_open(uint32_t *token)
{
    if (!token) return ESP_ERR_INVALID_ARG;
    if (!requests) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&mux);
    if (pending || atomic_load(&closing)) { portEXIT_CRITICAL(&mux); return ESP_ERR_INVALID_STATE; }
    uint32_t value = operations.next_token();
    if (!value) { portEXIT_CRITICAL(&mux); return ESP_ERR_INVALID_STATE; }
    unsigned slot = next_slot;
    results[slot].token = value; results[slot].complete = false; pending = true;
    next_slot = (next_slot + 1) % 8;
    portEXIT_CRITICAL(&mux);
    if (xQueueSend(requests, &slot, 0) != pdTRUE) {
        portENTER_CRITICAL(&mux); results[slot].token = 0; pending = false;
        portEXIT_CRITICAL(&mux); return ESP_ERR_INVALID_STATE;
    }
    *token = value; return ESP_OK;
}
esp_err_t app_core_factory_request(uint32_t *token)
{
    if (!token) return ESP_ERR_INVALID_ARG;
    atomic_fetch_add(&admissions, 1);
    esp_err_t error = atomic_load(&closing) ? ESP_ERR_INVALID_STATE : request_open(token);
    atomic_fetch_sub(&admissions, 1);
    return error;
}
bool app_core_factory_quiesce(uint32_t timeout_ms)
{
    atomic_store(&closing, true);
    if (!requests) return true;
    /* Full means an already admitted request will wake the original blocking
     * receive. The private sentinel never consumes a result-history slot. */
    unsigned wake = 8;
    if (atomic_load(&running)) xQueueSend(requests, &wake, 0);
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (atomic_load(&running) || atomic_load(&admissions)) {
        if (esp_timer_get_time() >= deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vQueueDelete(requests); requests = NULL;
    return true;
}
esp_err_t app_core_factory_result(uint32_t token, esp_err_t *result)
{
    if (!token || !result) return ESP_ERR_INVALID_ARG;
    esp_err_t state = ESP_ERR_NOT_FOUND;
    portENTER_CRITICAL(&mux);
    for (unsigned i = 0; i < 8; ++i) if (results[i].token == token) {
        state = results[i].complete ? ESP_OK : ESP_ERR_NOT_FINISHED;
        if (results[i].complete) *result = results[i].result;
        break;
    }
    portEXIT_CRITICAL(&mux); return state;
}
