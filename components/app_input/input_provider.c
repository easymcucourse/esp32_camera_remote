#include "input_provider_registry.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include <limits.h>
#include <string.h>

typedef struct {
    input_provider_handle_t handle;
    bool registered, disconnect_pending;
    input_disconnect_reason_t reason;
    uint64_t discard_through;
} provider_t;
typedef struct { input_provider_event_t event; uint64_t serial; } queued_t;
static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
static provider_t providers[INPUT_SOURCE_COUNT];
static queued_t reports[INPUT_PROVIDER_REPORT_CAPACITY];
static unsigned head, count;
static uint32_t next_handle = 1;
static uint64_t serial;
static bool initialized, accepting;

static provider_t *find(input_provider_handle_t handle)
{
    if (!handle) return NULL;
    for (unsigned i=0; i<INPUT_SOURCE_COUNT; ++i)
        if (providers[i].registered && providers[i].handle==handle) return &providers[i];
    return NULL;
}
static void disconnect(provider_t *p, input_disconnect_reason_t reason)
{
    p->disconnect_pending=true; p->reason=reason; p->discard_through=serial;
}
esp_err_t input_provider_registry_init(void)
{
    portENTER_CRITICAL(&mux);
    esp_err_t result=initialized ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (result==ESP_OK) { initialized=true; accepting=true; }
    portEXIT_CRITICAL(&mux); return result;
}
esp_err_t input_provider_registry_deinit(void)
{
    portENTER_CRITICAL(&mux);
    esp_err_t result=accepting ? ESP_ERR_INVALID_STATE : ESP_OK;
    for (unsigned i=0;i<INPUT_SOURCE_COUNT;++i)
        if (providers[i].registered) result=ESP_ERR_INVALID_STATE;
    if (result==ESP_OK) {
        memset(providers,0,sizeof(providers));head=count=0;initialized=false;
        /* Handles/serial remain monotonic across service lifetimes. */
    }
    portEXIT_CRITICAL(&mux);return result;
}
esp_err_t input_provider_register(input_source_kind_t kind, input_provider_handle_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    *out=0;
    if (kind<INPUT_SOURCE_ATOM || kind>=INPUT_SOURCE_COUNT) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&mux);
    provider_t *p=&providers[kind];
    esp_err_t result=!initialized || !accepting || !next_handle || p->registered ||
        p->disconnect_pending ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (result==ESP_OK) {
        *p=(provider_t){.handle=next_handle++, .registered=true, .discard_through=serial};
        *out=p->handle;
    }
    portEXIT_CRITICAL(&mux); return result;
}
esp_err_t input_provider_publish(input_provider_handle_t handle, const input_report_t *r)
{
    if (!r || !r->source_epoch || !r->report_id || r->rx<INT8_MIN || r->rx>INT8_MAX ||
        r->ry<INT8_MIN || r->ry>INT8_MAX || (r->battery>10 && r->battery!=255) ||
        r->kind>1 || r->gimbal>3 || r->buttons>0x3ffffu || r->event_buttons>0x3ffffu ||
        (r->connected && ((!r->atom_online && !r->sim) || r->mismatch)))
        return ESP_ERR_INVALID_ARG;
    uint32_t captured=(uint32_t)((uint64_t)esp_timer_get_time()/1000);
    portENTER_CRITICAL(&mux);
    provider_t *p=find(handle);
    esp_err_t result=!accepting || !p ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (result==ESP_OK && serial==UINT64_MAX) {
        accepting=false;
        for (unsigned i=0; i<INPUT_SOURCE_COUNT; ++i)
            if (providers[i].registered) disconnect(&providers[i],INPUT_DISCONNECT_STOP);
        result=ESP_ERR_INVALID_STATE;
    }
    if (result==ESP_OK) {
        ++serial;
        if (count==INPUT_PROVIDER_REPORT_CAPACITY) {
            disconnect(p,INPUT_DISCONNECT_OVERFLOW); result=ESP_ERR_NO_MEM;
        } else {
            unsigned tail=(head+count)%INPUT_PROVIDER_REPORT_CAPACITY;
            reports[tail]=(queued_t){.serial=serial,.event={.handle=handle,
                .source=(input_source_kind_t)(p-providers),.report=*r,.captured_ms=captured}};
            ++count;
        }
    }
    portEXIT_CRITICAL(&mux); return result;
}
esp_err_t input_provider_disconnect(input_provider_handle_t handle, input_disconnect_reason_t reason)
{
    if (reason<INPUT_DISCONNECT_OFFLINE || reason>=INPUT_DISCONNECT_REASON_COUNT) return ESP_ERR_INVALID_ARG;
    portENTER_CRITICAL(&mux);
    provider_t *p=find(handle);
    esp_err_t result=p ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (p) disconnect(p,reason);
    portEXIT_CRITICAL(&mux); return result;
}
esp_err_t input_provider_unregister(input_provider_handle_t handle)
{
    portENTER_CRITICAL(&mux);
    provider_t *p=find(handle);
    esp_err_t result=p ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (p) { disconnect(p,INPUT_DISCONNECT_STOP); p->registered=false; }
    portEXIT_CRITICAL(&mux); return result;
}
void input_provider_registry_close(void)
{
    portENTER_CRITICAL(&mux); accepting=false; head=count=0;
    for (unsigned i=0; i<INPUT_SOURCE_COUNT; ++i)
        if (providers[i].registered) disconnect(&providers[i],INPUT_DISCONNECT_STOP);
    portEXIT_CRITICAL(&mux);
}
bool input_provider_registry_next(input_provider_event_t *out)
{
    if (!out) return false;
    bool found=false;
    portENTER_CRITICAL(&mux);
    for (unsigned i=0; i<INPUT_SOURCE_COUNT; ++i) {
        provider_t *p=&providers[i];
        if (!p->disconnect_pending) continue;
        *out=(input_provider_event_t){.handle=p->handle,.source=(input_source_kind_t)i,
            .disconnected=true,.reason=p->reason};
        p->disconnect_pending=false; found=true; break;
    }
    while (!found && count) {
        queued_t item=reports[head]; head=(head+1)%INPUT_PROVIDER_REPORT_CAPACITY; --count;
        provider_t *p=&providers[item.event.source];
        if (!p->registered || item.event.handle!=p->handle || item.serial<=p->discard_through) continue;
        *out=item.event; found=true;
    }
    portEXIT_CRITICAL(&mux); return found;
}
bool input_provider_registry_idle(void)
{
    portENTER_CRITICAL(&mux);
    bool idle=count==0;
    for (unsigned i=0; i<INPUT_SOURCE_COUNT; ++i) idle=idle && !providers[i].disconnect_pending;
    portEXIT_CRITICAL(&mux); return idle;
}
