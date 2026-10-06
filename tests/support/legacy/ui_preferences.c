#include "ui_preferences.h"
#include "ui_overlay.h"
#include "async_token.h"
#include "app_ui.h"
#include "app_console.h"
#include "preferences_store.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
static const char *TAG="ui_preferences";
static QueueHandle_t requests,completed;
static SemaphoreHandle_t apply_lock;
static atomic_uint level,outstanding,pad_type;
static atomic_bool resetting;
static atomic_bool ready,closing,worker_running;
static atomic_uint admissions;
typedef struct { SemaphoreHandle_t done; esp_err_t error; } sync_t;
typedef struct {
    uint32_t token; unsigned value; app_ui_preference_op_t operation;
    bool legacy,typed,admitted; sync_t *sync; app_message_t origin;
} request_t;
typedef struct { uint32_t token; unsigned level; esp_err_t error; } result_t;
unsigned ui_preferences_level(void) { return atomic_load(&level); }
unsigned ui_preferences_pad(void) { return atomic_load(&pad_type); }
static esp_err_t save(const char *key,unsigned value) { return preferences_store_set(key,value); }
static esp_err_t reset_store(void) { return preferences_store_reset(); }
static esp_err_t submit_open(request_t *request,uint32_t *token)
{
    if (!ready) return ESP_ERR_INVALID_STATE;
    bool reset=request->operation==APP_UI_PREF_RESET;
    if (reset) {
        bool expected=false;
        if (!atomic_compare_exchange_strong(&resetting,&expected,true)) return ESP_ERR_INVALID_STATE;
    } else if (atomic_load(&resetting)) return ESP_ERR_INVALID_STATE;
    unsigned used=atomic_load(&outstanding);
    do {
        if (used>=8) { if (reset) atomic_store(&resetting,false); return ESP_ERR_INVALID_STATE; }
    } while (!atomic_compare_exchange_weak(&outstanding,&used,used+1));
    request->token=async_token_next();
    if (xQueueSend(requests,request,0)!=pdTRUE) {
        atomic_fetch_sub(&outstanding,1);
        if (reset) atomic_store(&resetting,false);
        return ESP_ERR_INVALID_STATE;
    }
    if (token) *token=request->token;
    return ESP_OK;
}
static esp_err_t submit(request_t *request,uint32_t *token)
{
    atomic_fetch_add(&admissions,1);
    esp_err_t result=atomic_load(&closing) ? ESP_ERR_INVALID_STATE : submit_open(request,token);
    atomic_fetch_sub(&admissions,1);
    return result;
}
static void result_payload(app_message_t *reply,const request_t *request)
{
    reply->payload.command.index=request->operation;
    reply->payload.command.token=request->token;
    reply->payload.command.value=ui_preferences_level();
    reply->payload.command.direction=(int32_t)ui_preferences_pad();
}
static void worker(void *context)
{
    (void)context;request_t request;
    for (;;) {
        if (atomic_load(&closing) && uxQueueMessagesWaiting(requests)==0) {
            if (!atomic_load(&admissions)) break;
            vTaskDelay(1);continue;
        }
        if (xQueueReceive(requests,&request,portMAX_DELAY)!=pdTRUE) continue;
        if (request.operation==APP_UI_PREF_COUNT && !request.token) continue; /* private stop wake */
        esp_err_t err=ESP_OK;
        if (atomic_load(&closing)) err=ESP_ERR_INVALID_STATE;
        else if (request.typed && request.origin.deadline_us<=esp_timer_get_time()) err=ESP_ERR_TIMEOUT;
        else if (request.typed && request.origin.endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI)) err=ESP_ERR_INVALID_STATE;
        else if (request.admitted && request.origin.generation!=app_console_endpoint_generation(APP_ENDPOINT_UART)) err=ESP_ERR_INVALID_STATE;
        xSemaphoreTake(apply_lock,portMAX_DELAY);
        unsigned value=request.operation==APP_UI_PREF_INFO_NEXT ? (ui_preferences_level()+1)%3 : request.value;
        if (atomic_load(&closing)) err=ESP_ERR_INVALID_STATE;
        if (err==ESP_OK && request.operation!=APP_UI_PREF_RESET && atomic_load(&resetting)) err=ESP_ERR_INVALID_STATE;
        if (err==ESP_OK) {
            if (request.operation==APP_UI_PREF_RESET) {
                err=reset_store();
                if (err==ESP_OK) {
                    atomic_store(&level,UI_INFO_FULL);atomic_store(&pad_type,0);app_ui_set_info_level(UI_INFO_FULL);
                }
            } else if (request.operation==APP_UI_PREF_PAD_SET) {
                err=save("pad",value);if (err==ESP_OK) atomic_store(&pad_type,value);
            } else {
                err=save("info",value);
                if (err==ESP_OK) { atomic_store(&level,value);app_ui_set_info_level(value); }
            }
        }
        if (err!=ESP_OK && request.operation==APP_UI_PREF_RESET) atomic_store(&resetting,false);
        xSemaphoreGive(apply_lock);
        if (request.legacy && atomic_load(&closing)) {
            atomic_fetch_sub(&outstanding,1);
        } else if (request.legacy) {
            result_t result={request.token,ui_preferences_level(),err};
            BaseType_t sent=xQueueSend(completed,&result,0);configASSERT(sent==pdTRUE);
        } else {
            if (request.sync) {
                sync_t *sync=request.sync;sync->error=err;
                /* Last context access: caller may release it after Give. */
                xSemaphoreGive(sync->done);
            } else {
                app_message_t result={.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_UI,
                    .flags=APP_MESSAGE_EVENT,.generation=app_console_endpoint_generation(APP_ENDPOINT_UI),.result=err};
                result_payload(&result,&request);
                if (request.admitted) {
                    /* Keep this completion and its capacity reservation while
                     * UART is full. Closing its lifetime retires the result. */
                    while (!atomic_load(&closing) && request.origin.generation==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
                        request.origin.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI)) {
                        app_message_t delivery=result;
                        if (app_console_send(&delivery)==ESP_OK) break;
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                } else if (request.origin.flags&APP_MESSAGE_REQUEST) app_console_reply(&request.origin,&result);
                else app_console_send(&result);
            }
            atomic_fetch_sub(&outstanding,1);
        }
    }
    atomic_store(&worker_running,false);
    vTaskDelete(NULL);
}
esp_err_t ui_preferences_start(void)
{
    if (atomic_load(&ready)) return ESP_OK;
    if (requests || atomic_load(&worker_running)) return ESP_ERR_INVALID_STATE;
    atomic_store(&closing,false);atomic_store(&resetting,false);
    preferences_config_t saved={PREFERENCES_CONFIG_VERSION,0,0};
    esp_err_t error=preferences_store_load(&saved);
    if(error!=ESP_OK)ESP_LOGW(TAG,"Invalid/unavailable preference (%s); full display",esp_err_to_name(error));
    unsigned value=saved.info_level;
    atomic_store(&level,value);app_ui_set_info_level(value);
    atomic_store(&pad_type,saved.controller);
    requests=xQueueCreate(4,sizeof(request_t));completed=xQueueCreate(8,sizeof(result_t));apply_lock=xSemaphoreCreateMutex();
    atomic_store(&worker_running,true);
    if (!requests || !completed || !apply_lock || xTaskCreate(worker,"ui_preferences",3072,NULL,2,NULL)!=pdPASS) {
        if (requests) vQueueDelete(requests);
        if (completed) vQueueDelete(completed);
        if (apply_lock) vSemaphoreDelete(apply_lock);
        requests=completed=NULL;apply_lock=NULL;atomic_store(&worker_running,false);return ESP_ERR_NO_MEM;
    }
    ready=true;ESP_LOGI(TAG,"INFO %s loaded",ui_info_name(value));return ESP_OK;
}
esp_err_t ui_preferences_request(unsigned value,bool next,uint32_t *token)
{
    if (token) *token=0;
    if (!token || (!next && value>UI_INFO_HIDDEN)) return ESP_ERR_INVALID_ARG;
    request_t request={.value=value,.legacy=true,.operation=next ? APP_UI_PREF_INFO_NEXT : APP_UI_PREF_INFO_SET};
    return submit(&request,token);
}
static esp_err_t synchronous(app_ui_preference_op_t operation,unsigned value)
{
    sync_t sync={.done=xSemaphoreCreateBinary()};
    if (!sync.done) return ESP_ERR_NO_MEM;
    request_t request={.operation=operation,.value=value,.sync=&sync};
    esp_err_t err=submit(&request,NULL);
    if (err==ESP_OK) { xSemaphoreTake(sync.done,portMAX_DELAY);err=sync.error; }
    vSemaphoreDelete(sync.done);return err;
}
esp_err_t ui_preferences_set_pad(unsigned mode)
{
    if (mode>1) return ESP_ERR_INVALID_ARG;
    return synchronous(APP_UI_PREF_PAD_SET,mode);
}
static bool result_open(uint32_t *token,unsigned *value,esp_err_t *error)
{
    if (!ready || !token || !value || !error) return false;
    result_t result;
    if (xQueueReceive(completed,&result,0)!=pdTRUE) return false;
    *token=result.token;*value=result.level;*error=result.error;
    atomic_fetch_sub(&outstanding,1);return true;
}
bool ui_preferences_result(uint32_t *token,unsigned *value,esp_err_t *error)
{
    atomic_fetch_add(&admissions,1);
    bool result=!atomic_load(&closing) && result_open(token,value,error);
    atomic_fetch_sub(&admissions,1);return result;
}
bool ui_preferences_quiesce(uint32_t timeout_ms)
{
    atomic_store(&closing,true);atomic_store(&ready,false);
    if (requests && atomic_load(&worker_running)) {
        request_t wake={.operation=APP_UI_PREF_COUNT};
        xQueueSend(requests,&wake,0); /* Full means queued work already wakes the worker. */
    }
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&worker_running) || atomic_load(&admissions)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    result_t result;
    while (completed && xQueueReceive(completed,&result,0)==pdTRUE) atomic_fetch_sub(&outstanding,1);
    if (atomic_load(&outstanding)) return false;
    if (requests) vQueueDelete(requests);
    if (completed) vQueueDelete(completed);
    if (apply_lock) vSemaphoreDelete(apply_lock);
    requests=completed=NULL;apply_lock=NULL;return true;
}
esp_err_t ui_preferences_message(const app_message_t *m,app_message_t *reply,bool *deferred)
{
    if (!m || !reply || !deferred || m->lease || m->type!=APP_MESSAGE_UI_PREFERENCES ||
        (m->flags!=0 && m->flags!=APP_MESSAGE_REQUEST) || !m->generation ||
        (m->source!=APP_ENDPOINT_INPUT && m->source!=APP_ENDPOINT_UART &&
         m->source!=APP_ENDPOINT_SYSTEM && m->source!=APP_ENDPOINT_INPUT_ATOM)) return ESP_ERR_INVALID_ARG;
    *deferred=false;
    unsigned op=m->payload.command.index,value=m->payload.command.value;
    bool admitted=m->payload.command.flag;
    if (admitted && (m->source!=APP_ENDPOINT_UART || m->flags!=APP_MESSAGE_REQUEST ||
        op==APP_UI_PREF_GET || op==APP_UI_PREF_RESET)) return ESP_ERR_INVALID_ARG;
    if (op>=APP_UI_PREF_COUNT || (op==APP_UI_PREF_INFO_SET && value>UI_INFO_HIDDEN) ||
        (op==APP_UI_PREF_PAD_SET && value>1) || (op==APP_UI_PREF_RESET && m->source!=APP_ENDPOINT_SYSTEM))
        return ESP_ERR_INVALID_ARG;
    if (!ready) return ESP_ERR_INVALID_STATE;
    if (m->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (op==APP_UI_PREF_GET) {
        reply->payload.command.value=ui_preferences_level();reply->payload.command.direction=(int32_t)ui_preferences_pad();return ESP_OK;
    }
    request_t request={.operation=(app_ui_preference_op_t)op,.value=value,.typed=true,.admitted=admitted,.origin=*m};
    uint32_t token=0;esp_err_t err=submit(&request,&token);
    if (err==ESP_OK) {
        *deferred=!admitted;
        if (admitted) reply->payload.command.token=token;
    }
    return err;
}
