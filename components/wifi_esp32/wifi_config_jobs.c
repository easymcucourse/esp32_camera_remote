#include "wifi_config_jobs.h"
#include "wifi_apply.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <stdlib.h>

typedef struct { network_config_t config; uint32_t token; bool staged; } request_t;
typedef struct {
    uint32_t token, ready_ms;
    app_wifi_result_t result;
    bool used, complete, staged, released, cancelled;
} result_t;
struct wifi_config_jobs {
    QueueHandle_t queue;
    SemaphoreHandle_t enqueue;
    portMUX_TYPE mux;
    network_config_t current;
    unsigned max_channel;
    result_t results[8];
    bool frozen, stopping;
    atomic_bool running;
    void *context;
    wifi_job_io_t save, restart;
    app_wifi_result_t save_error;
};
static bool save(void *context, const network_config_t *config)
{
    wifi_config_jobs_t *j = context;
    j->save_error = j->save(j->context, config); return j->save_error == APP_WIFI_OK;
}
static bool restart(void *context, const network_config_t *config)
{ wifi_config_jobs_t *j = context; return j->restart(j->context, config) == APP_WIFI_OK; }
static void complete(wifi_config_jobs_t *j, uint32_t token, app_wifi_result_t error)
{
    portENTER_CRITICAL(&j->mux);
    for (unsigned i = 0; i < 8; ++i) if (j->results[i].used && j->results[i].token == token) {
        j->results[i].result = error; j->results[i].complete = true; break;
    }
    portEXIT_CRITICAL(&j->mux);
}
static app_wifi_result_t wait_staged(wifi_config_jobs_t *j, uint32_t token)
{
    uint32_t started = (uint32_t)(esp_timer_get_time() / 1000);
    for (;;) {
        result_t entry = {0}; bool stopping;
        portENTER_CRITICAL(&j->mux); stopping = j->stopping;
        for (unsigned i = 0; i < 8; ++i) if (j->results[i].used && j->results[i].token == token) entry = j->results[i];
        portEXIT_CRITICAL(&j->mux);
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        if (!entry.used || entry.cancelled || stopping) return APP_WIFI_CANCELLED;
        if (entry.released && (int32_t)(now - entry.ready_ms) >= 0) return APP_WIFI_OK;
        if ((uint32_t)(now - started) >= 25000) return APP_WIFI_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
static void task(void *context)
{
    wifi_config_jobs_t *j = context;
    for (;;) {
        portENTER_CRITICAL(&j->mux); bool stopping = j->stopping; portEXIT_CRITICAL(&j->mux);
        if (stopping) break;
        request_t request;
        if (xQueueReceive(j->queue, &request, pdMS_TO_TICKS(200)) != pdTRUE) continue;
        app_wifi_result_t error = request.staged ? wait_staged(j, request.token) : APP_WIFI_OK;
        portENTER_CRITICAL(&j->mux); stopping = j->stopping; portEXIT_CRITICAL(&j->mux);
        if (stopping) error = APP_WIFI_CANCELLED;
        if (error == APP_WIFI_OK) {
            network_config_t current; wifi_jobs_get(j, &current);
            wifi_apply_result_t applied = wifi_apply_config(&current, &request.config, j->max_channel, save, restart, j);
            error = applied == WIFI_APPLY_OK ? APP_WIFI_OK : applied == WIFI_APPLY_INVALID ? APP_WIFI_INVALID :
                applied == WIFI_APPLY_SAVE_FAILED ? j->save_error : APP_WIFI_IO;
            if (applied == WIFI_APPLY_OK) {
                portENTER_CRITICAL(&j->mux); j->current = current; portEXIT_CRITICAL(&j->mux);
            }
        }
        complete(j, request.token, error);
    }
    request_t request;
    while (xQueueReceive(j->queue, &request, 0) == pdTRUE) complete(j, request.token, APP_WIFI_CANCELLED);
    atomic_store(&j->running, false);
    vTaskDelete(NULL);
}
wifi_config_jobs_t *wifi_jobs_create(void *context, wifi_job_io_t save_fn, wifi_job_io_t restart_fn)
{
    if (!context || !save_fn || !restart_fn) return NULL;
    wifi_config_jobs_t *j = calloc(1, sizeof(*j)); if (!j) return NULL;
    j->context = context; j->save = save_fn; j->restart = restart_fn;
    j->mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED; j->max_channel = 13;
    atomic_init(&j->running, false); network_config_make_default(&j->current);
    j->queue = xQueueCreate(2, sizeof(request_t)); j->enqueue = xSemaphoreCreateMutex();
    if (!j->queue || !j->enqueue) { wifi_jobs_destroy(j); return NULL; }
    return j;
}
void wifi_jobs_destroy(wifi_config_jobs_t *j)
{
    if (!j) return;
    if (j->queue) vQueueDelete(j->queue);
    if (j->enqueue) vSemaphoreDelete(j->enqueue);
    free(j);
}
void wifi_jobs_set(wifi_config_jobs_t *j, const network_config_t *config, unsigned limit)
{ portENTER_CRITICAL(&j->mux); j->current = *config; j->max_channel = limit; portEXIT_CRITICAL(&j->mux); }
app_wifi_result_t wifi_jobs_get(wifi_config_jobs_t *j, network_config_t *config)
{ portENTER_CRITICAL(&j->mux); *config = j->current; portEXIT_CRITICAL(&j->mux); return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_start(wifi_config_jobs_t *j)
{
    if (atomic_exchange(&j->running, true)) return APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux); j->stopping = false; j->frozen = false; portEXIT_CRITICAL(&j->mux);
    if (xTaskCreate(task, "wifi_config", 4096, j, 2, NULL) != pdPASS) {
        portENTER_CRITICAL(&j->mux);j->stopping=true;j->frozen=true;portEXIT_CRITICAL(&j->mux);
        atomic_store(&j->running, false); return APP_WIFI_NO_MEMORY;
    }
    return APP_WIFI_OK;
}
app_wifi_result_t wifi_jobs_apply(wifi_config_jobs_t *j, const network_config_t *config, bool staged, uint32_t *token)
{
    if (network_config_check(config, j->max_channel) != NETWORK_CFG_OK || !token) return APP_WIFI_INVALID;
    if (xSemaphoreTake(j->enqueue, 0) != pdTRUE) return APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux);
    if (j->frozen || j->stopping || !atomic_load(&j->running)) {
        portEXIT_CRITICAL(&j->mux); xSemaphoreGive(j->enqueue); return APP_WIFI_STATE;
    }
    unsigned slot = 8;
    for (unsigned i = 0; i < 8; ++i) if (!j->results[i].used) { slot = i; break; }
    if (slot == 8) for (unsigned i = 0; i < 8; ++i) if (j->results[i].complete &&
        (slot == 8 || (int32_t)(j->results[i].token - j->results[slot].token) < 0)) slot = i;
    if (slot == 8) { portEXIT_CRITICAL(&j->mux); xSemaphoreGive(j->enqueue); return APP_WIFI_STATE; }
    request_t request = {.config = *config, .staged = staged, .token = app_wifi_next_token()};
    j->results[slot] = (result_t){.token = request.token, .used = true, .staged = staged};
    portEXIT_CRITICAL(&j->mux);
    if (xQueueSend(j->queue, &request, 0) != pdTRUE) {
        portENTER_CRITICAL(&j->mux); j->results[slot].used = false; portEXIT_CRITICAL(&j->mux);
        xSemaphoreGive(j->enqueue); return APP_WIFI_STATE;
    }
    *token = request.token; xSemaphoreGive(j->enqueue); return APP_WIFI_OK;
}
app_wifi_result_t wifi_jobs_commit(wifi_config_jobs_t *j, uint32_t token, unsigned delay_ms)
{
    if (!token || delay_ms > 10000) return APP_WIFI_INVALID;
    app_wifi_result_t error = APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux);
    for (unsigned i = 0; i < 8; ++i) if (!j->stopping && j->results[i].used && j->results[i].token == token &&
        j->results[i].staged && !j->results[i].complete && !j->results[i].released && !j->results[i].cancelled) {
        j->results[i].ready_ms = (uint32_t)(esp_timer_get_time() / 1000) + delay_ms;
        j->results[i].released = true; error = APP_WIFI_OK; break;
    }
    portEXIT_CRITICAL(&j->mux); return error;
}
app_wifi_result_t wifi_jobs_cancel(wifi_config_jobs_t *j, uint32_t token)
{
    if (!token) return APP_WIFI_INVALID;
    app_wifi_result_t error = APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux);
    for (unsigned i = 0; i < 8; ++i) if (j->results[i].used && j->results[i].token == token &&
        j->results[i].staged && !j->results[i].released && !j->results[i].complete) {
        j->results[i].cancelled = true; error = APP_WIFI_OK; break;
    }
    portEXIT_CRITICAL(&j->mux); return error;
}
app_wifi_result_t wifi_jobs_result(wifi_config_jobs_t *j, uint32_t token, app_wifi_result_t *result)
{
    if (!token || !result) return APP_WIFI_INVALID;
    app_wifi_result_t state = APP_WIFI_NOT_FOUND;
    portENTER_CRITICAL(&j->mux);
    for (unsigned i = 0; i < 8; ++i) if (j->results[i].used && j->results[i].token == token) {
        state = j->results[i].complete ? APP_WIFI_OK : APP_WIFI_PENDING;
        if (j->results[i].complete) *result = j->results[i].result;
        break;
    }
    portEXIT_CRITICAL(&j->mux); return state;
}
app_wifi_result_t wifi_jobs_freeze(wifi_config_jobs_t *j, uint32_t timeout_ms)
{
    if (xSemaphoreTake(j->enqueue, 0) != pdTRUE) return APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux);
    bool unavailable = j->frozen || j->stopping;
    if (!unavailable) j->frozen = true;
    portEXIT_CRITICAL(&j->mux); xSemaphoreGive(j->enqueue);
    if (unavailable) return APP_WIFI_STATE;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (;;) {
        bool idle = true;
        portENTER_CRITICAL(&j->mux);
        for (unsigned i = 0; i < 8; ++i) if (j->results[i].used && !j->results[i].complete) idle = false;
        portEXIT_CRITICAL(&j->mux);
        if (idle) return APP_WIFI_OK;
        if (esp_timer_get_time() >= deadline) { wifi_jobs_resume(j); return APP_WIFI_TIMEOUT; }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void wifi_jobs_resume(wifi_config_jobs_t *j)
{ portENTER_CRITICAL(&j->mux); if (!j->stopping) j->frozen = false; portEXIT_CRITICAL(&j->mux); }
app_wifi_result_t wifi_jobs_stop(wifi_config_jobs_t *j, uint32_t timeout_ms)
{
    if (xSemaphoreTake(j->enqueue, 0) != pdTRUE) return APP_WIFI_STATE;
    portENTER_CRITICAL(&j->mux); j->stopping = true; j->frozen = true; portEXIT_CRITICAL(&j->mux);
    xSemaphoreGive(j->enqueue);
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (atomic_load(&j->running)) {
        if (esp_timer_get_time() >= deadline) return APP_WIFI_TIMEOUT;
        vTaskDelay(1);
    }
    return APP_WIFI_OK;
}
