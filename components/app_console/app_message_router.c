#include "app_console.h"
#include "app_message_internal.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdatomic.h>
#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "freertos/idf_additions.h"
#endif

/* Task-only copied envelopes use PSRAM. Keep the FreeRTOS queue control block
 * in internal RAM so scheduler bookkeeping remains cache independent. Payload
 * copies are task-only; ISR paths never access these endpoint queues. */
static QueueHandle_t endpoint_queue_create(unsigned depth)
{
#ifdef ESP_PLATFORM
    StaticQueue_t *control = heap_caps_malloc(sizeof(*control), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    uint8_t *storage = heap_caps_malloc(depth * sizeof(app_message_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    QueueHandle_t queue = control && storage ? xQueueCreateStatic(depth, sizeof(app_message_t), storage, control) : NULL;
    if (!queue) { heap_caps_free(storage); heap_caps_free(control); }
    return queue;
#else
    return xQueueCreate(depth, sizeof(app_message_t));
#endif
}
static void endpoint_queue_delete(QueueHandle_t queue)
{
#ifdef ESP_PLATFORM
    /* The SDK retrieves and frees both buffers, irrespective of their caps. */
    vQueueDeleteWithCaps(queue);
#else
    vQueueDelete(queue);
#endif
}

typedef struct {
    QueueHandle_t control, bulk;
    app_endpoint_config_t config;
    uint32_t epoch;
    bool active;
} endpoint_t;
typedef struct {
    bool used, complete;
    app_endpoint_t source, target;
    app_message_type_t type;
    uint32_t correlation, generation, source_epoch, target_epoch;
    int64_t deadline;
    esp_err_t result;
    app_message_t reply;
    SemaphoreHandle_t wake;
} pending_t;

static SemaphoreHandle_t mutex;
static endpoint_t endpoints[APP_ENDPOINT_COUNT];
static pending_t pending[APP_CONSOLE_PENDING_CAPACITY];
static uint32_t subscriptions[APP_MESSAGE_COUNT], next_correlation, worker_epoch;
static app_console_status_t status;
static atomic_bool housekeeping_running;

static bool endpoint_valid(app_endpoint_t endpoint)
{ return endpoint > APP_ENDPOINT_NONE && endpoint < APP_ENDPOINT_COUNT; }
static bool expired(const app_message_t *message)
{ return message->deadline_us && esp_timer_get_time() >= message->deadline_us; }
static TickType_t deadline_ticks(int64_t deadline)
{
    int64_t remaining_us = deadline - esp_timer_get_time();
    if (remaining_us <= 0) return 0;
    uint64_t ticks = ((uint64_t)remaining_us + portTICK_PERIOD_MS * 1000u - 1u) /
                     (portTICK_PERIOD_MS * 1000u);
    return ticks >= portMAX_DELAY ? portMAX_DELAY - 1 : (TickType_t)ticks;
}

/* mutex is held; no user destructors run here. Waiter slots remain allocated
 * until their owning request task acknowledges cancellation. */
static void cancel_pending(app_endpoint_t endpoint, esp_err_t error)
{
    for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i) {
        pending_t *p = &pending[i];
        if (!p->used || p->complete) continue;
        if (endpoint != APP_ENDPOINT_NONE && p->source != endpoint && p->target != endpoint) continue;
        p->result = error; p->complete = true;
        xSemaphoreGive(p->wake);
    }
}

static void purge_queue(QueueHandle_t queue, app_endpoint_t id)
{
    if (!queue) return;
    app_message_lease_t *released[64];
    unsigned release_count = 0;
    xSemaphoreTake(mutex, portMAX_DELAY);
    uint32_t epoch = endpoints[id].epoch;
    bool discard_all = !endpoints[id].active;
    unsigned count = uxQueueMessagesWaiting(queue);
    for (unsigned i = 0; i < count; ++i) {
        app_message_t message;
        if (xQueueReceive(queue, &message, 0) != pdTRUE) break;
        if (discard_all || message.endpoint_epoch != epoch || expired(&message)) {
            if (message.lease) released[release_count++] = message.lease;
            ++status.expired;
        } else if (xQueueSend(queue, &message, 0) != pdTRUE) {
            if (message.lease) released[release_count++] = message.lease;
            ++status.rejected;
        }
    }
    xSemaphoreGive(mutex);
    for (unsigned i = 0; i < release_count; ++i) {
        app_message_t message = {.lease = released[i]};
        app_message_release(&message);
    }
}

void app_console_router_poll(void)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    int64_t now = esp_timer_get_time();
    for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i) {
        pending_t *p = &pending[i];
        if (p->used && !p->complete && now >= p->deadline) {
            p->complete = true; p->result = ESP_ERR_TIMEOUT;
            ++status.expired; xSemaphoreGive(p->wake);
        }
    }
    xSemaphoreGive(mutex);
    for (unsigned i = 1; i < APP_ENDPOINT_COUNT; ++i) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        endpoint_t snapshot = endpoints[i];
        xSemaphoreGive(mutex);
        purge_queue(snapshot.control, (app_endpoint_t)i);
        purge_queue(snapshot.bulk, (app_endpoint_t)i);
    }
}

static void router_housekeeping(void *argument)
{
    uint32_t epoch = (uint32_t)(uintptr_t)argument;
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool running = status.running && worker_epoch == epoch;
        xSemaphoreGive(mutex);
        if (!running) break;
        app_console_router_poll();
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    atomic_store(&housekeeping_running, false);
    vTaskDelete(NULL);
}

esp_err_t app_console_router_start(void)
{
    if (!mutex) {
        mutex = xSemaphoreCreateMutex();
        if (!mutex) return ESP_ERR_NO_MEM;
        for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i) {
            pending[i].wake = xSemaphoreCreateBinary();
            if (!pending[i].wake) {
                for (unsigned j = 0; j < i; ++j) vSemaphoreDelete(pending[j].wake);
                memset(pending, 0, sizeof(pending));
                vSemaphoreDelete(mutex); mutex = NULL;
                return ESP_ERR_NO_MEM;
            }
        }
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (status.running || atomic_load(&housekeeping_running) || status.pending || app_message_lease_count()) {
        xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE;
    }
    memset(subscriptions, 0, sizeof(subscriptions));
    status.running = status.accepting = true; status.subscriptions_frozen = false;
    uint32_t epoch = ++worker_epoch;
    atomic_store(&housekeeping_running, true);
    xSemaphoreGive(mutex);
    if (xTaskCreate(router_housekeeping, "message_router", 4096,
                    (void *)(uintptr_t)epoch, 5, NULL) != pdPASS) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        status.running = status.accepting = false;
        atomic_store(&housekeeping_running, false);
        xSemaphoreGive(mutex); return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t app_console_endpoint_register(app_endpoint_t id, const app_endpoint_config_t *config)
{
    if (!endpoint_valid(id) || !config || !config->control_depth || !config->bulk_depth ||
        config->control_depth > 64 || config->bulk_depth > 64) return ESP_ERR_INVALID_ARG;
    if (!mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    endpoint_t *endpoint = &endpoints[id];
    if (!status.accepting || endpoint->active ||
        (status.subscriptions_frozen && !endpoint->control)) {
        xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE;
    }
    if (endpoint->control) {
        if (endpoint->config.control_depth != config->control_depth || endpoint->config.bulk_depth != config->bulk_depth) {
            xSemaphoreGive(mutex); return ESP_ERR_INVALID_ARG;
        }
    } else {
        endpoint->control = endpoint_queue_create(config->control_depth);
        endpoint->bulk = endpoint_queue_create(config->bulk_depth);
        if (!endpoint->control || !endpoint->bulk) {
            if (endpoint->control) endpoint_queue_delete(endpoint->control);
            if (endpoint->bulk) endpoint_queue_delete(endpoint->bulk);
            endpoint->control = endpoint->bulk = NULL;
            xSemaphoreGive(mutex); return ESP_ERR_NO_MEM;
        }
        endpoint->config = *config;
    }
    if (++endpoint->epoch == 0) ++endpoint->epoch;
    endpoint->active = true; ++status.endpoints;
    xSemaphoreGive(mutex); return ESP_OK;
}

void app_console_endpoint_stop(app_endpoint_t id)
{
    if (!mutex || !endpoint_valid(id)) return;
    xSemaphoreTake(mutex, portMAX_DELAY);
    endpoint_t *endpoint = &endpoints[id];
    if (endpoint->active) { endpoint->active = false; --status.endpoints; ++endpoint->epoch; }
    cancel_pending(id, ESP_ERR_INVALID_STATE);
    QueueHandle_t control = endpoint->control, bulk = endpoint->bulk;
    xSemaphoreGive(mutex);
    purge_queue(control, id); purge_queue(bulk, id);
}

uint32_t app_console_endpoint_generation(app_endpoint_t id)
{
    if (!mutex || !endpoint_valid(id)) return 0;
    xSemaphoreTake(mutex, portMAX_DELAY);
    uint32_t epoch = endpoints[id].active ? endpoints[id].epoch : 0;
    xSemaphoreGive(mutex); return epoch;
}

esp_err_t app_console_subscribe(app_message_type_t type, app_endpoint_t subscriber)
{
    if ((unsigned)type >= APP_MESSAGE_COUNT || !endpoint_valid(subscriber)) return ESP_ERR_INVALID_ARG;
    if (!mutex) return ESP_ERR_INVALID_STATE;
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (!status.accepting || status.subscriptions_frozen || !endpoints[subscriber].active) {
        xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE;
    }
    subscriptions[type] |= 1u << subscriber;
    xSemaphoreGive(mutex); return ESP_OK;
}

void app_console_freeze_subscriptions(void)
{
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY); status.subscriptions_frozen = true;
    xSemaphoreGive(mutex);
}

static esp_err_t route_reply(app_message_t *message)
{
    for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i) {
        pending_t *p = &pending[i];
        if (!p->used || p->complete || p->correlation != message->correlation_id ||
            p->source != message->target || p->target != message->source ||
            p->type != message->type || p->generation != message->generation ||
            endpoints[p->source].epoch != p->source_epoch ||
            endpoints[p->target].epoch != p->target_epoch ||
            esp_timer_get_time() >= p->deadline) continue;
        p->reply = *message; message->lease = NULL;
        p->complete = true; p->result = ESP_OK; xSemaphoreGive(p->wake);
        return ESP_OK;
    }
    ++status.late_replies; return ESP_ERR_INVALID_STATE;
}

static esp_err_t send_with_lock_wait(app_message_t *message, TickType_t lock_wait)
{
    if (!message) return ESP_ERR_INVALID_ARG;
    unsigned kind = message->flags & (APP_MESSAGE_REQUEST | APP_MESSAGE_REPLY | APP_MESSAGE_EVENT);
    if ((unsigned)message->type >= APP_MESSAGE_COUNT || !endpoint_valid(message->source) || !message->generation ||
        (message->flags & ~(APP_MESSAGE_REQUEST | APP_MESSAGE_REPLY | APP_MESSAGE_EVENT | APP_MESSAGE_BULK)) ||
        ((kind == APP_MESSAGE_REQUEST || kind == APP_MESSAGE_REPLY) &&
            (!message->correlation_id || message->deadline_us <= 0)) ||
        (kind && kind != APP_MESSAGE_REQUEST && kind != APP_MESSAGE_REPLY && kind != APP_MESSAGE_EVENT) ||
        ((message->flags & APP_MESSAGE_BULK) && !message->lease) ||
        (kind != APP_MESSAGE_EVENT && !endpoint_valid(message->target))) {
        app_message_release(message); return ESP_ERR_INVALID_ARG;
    }
    if (!mutex) { app_message_release(message); return ESP_ERR_INVALID_STATE; }
    if (xSemaphoreTake(mutex, lock_wait) != pdTRUE) { app_message_release(message); return ESP_ERR_TIMEOUT; }
    esp_err_t error = ESP_OK;
    if (!status.accepting || !endpoints[message->source].active) error = ESP_ERR_INVALID_STATE;
    else if (expired(message)) error = ESP_ERR_TIMEOUT;
    else if (kind==APP_MESSAGE_REQUEST) {
        /* Admission and enqueue are separate lock intervals. Reject a request
         * if either lifetime was retired between them; never execute it on a
         * newly registered endpoint with an old correlation reservation. */
        bool current=false;
        for (unsigned i=0;i<APP_CONSOLE_PENDING_CAPACITY;++i) {
            pending_t *p=&pending[i];
            if (p->used && !p->complete && p->correlation==message->correlation_id &&
                p->source==message->source && p->target==message->target && p->type==message->type &&
                p->generation==message->generation && p->source_epoch==endpoints[p->source].epoch &&
                p->target_epoch==endpoints[p->target].epoch && endpoints[p->target].active) { current=true;break; }
        }
        if (!current) error=ESP_ERR_INVALID_STATE;
        else {
            endpoint_t *endpoint=&endpoints[message->target];
            app_message_t delivery=*message;delivery.endpoint_epoch=endpoint->epoch;
            QueueHandle_t queue=message->flags&APP_MESSAGE_BULK ? endpoint->bulk : endpoint->control;
            if (xQueueSend(queue,&delivery,0)!=pdTRUE) error=ESP_ERR_TIMEOUT;
            else message->lease=NULL;
        }
    }
    else if (kind == APP_MESSAGE_REPLY) error = route_reply(message);
    else if (kind == APP_MESSAGE_EVENT) {
        uint32_t subscribers = subscriptions[message->type];
        for (unsigned i = 1; i < APP_ENDPOINT_COUNT; ++i) if (subscribers & (1u << i)) {
            endpoint_t *endpoint = &endpoints[i];
            if (!endpoint->active) continue;
            app_message_t delivery = *message;
            delivery.target = (app_endpoint_t)i; delivery.endpoint_epoch = endpoint->epoch;
            if (delivery.lease && !app_message_lease_retain(delivery.lease)) { error = ESP_ERR_INVALID_STATE; continue; }
            QueueHandle_t queue = delivery.flags & APP_MESSAGE_BULK ? endpoint->bulk : endpoint->control;
            if (xQueueSend(queue, &delivery, 0) != pdTRUE) {
                /* Producer's reference remains, so this decrement cannot run a destructor. */
                app_message_release(&delivery); error = ESP_ERR_TIMEOUT;
            }
        }
    } else {
        endpoint_t *endpoint = &endpoints[message->target];
        if (!endpoint->active) error = ESP_ERR_INVALID_STATE;
        else {
            app_message_t delivery = *message; delivery.endpoint_epoch = endpoint->epoch;
            QueueHandle_t queue = message->flags & APP_MESSAGE_BULK ? endpoint->bulk : endpoint->control;
            if (xQueueSend(queue, &delivery, 0) != pdTRUE) error = ESP_ERR_TIMEOUT;
            else message->lease = NULL;
        }
    }
    if (error == ESP_OK) ++status.sent; else ++status.rejected;
    xSemaphoreGive(mutex); app_message_release(message);
    return error;
}

esp_err_t app_console_send(app_message_t *message)
{ return send_with_lock_wait(message, 0); }

esp_err_t app_console_receive(app_endpoint_t id, app_message_t *message, uint32_t timeout_ms)
{
    if (!endpoint_valid(id) || !message) return ESP_ERR_INVALID_ARG;
    if (!mutex) return ESP_ERR_INVALID_STATE;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (;;) {
        if (xSemaphoreTake(mutex, deadline_ticks(deadline)) != pdTRUE) return ESP_ERR_TIMEOUT;
        endpoint_t *endpoint = &endpoints[id];
        if (!status.accepting || !endpoint->active) { xSemaphoreGive(mutex); return ESP_ERR_INVALID_STATE; }
        bool received = xQueueReceive(endpoint->control, message, 0) == pdTRUE ||
                        xQueueReceive(endpoint->bulk, message, 0) == pdTRUE;
        bool valid = received && message->endpoint_epoch == endpoint->epoch && !expired(message);
        if (received && !valid) ++status.expired;
        xSemaphoreGive(mutex);
        if (valid) return ESP_OK;
        if (received) { app_message_release(message); continue; }
        if (esp_timer_get_time() >= deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(1);
    }
}

static esp_err_t begin_request(app_message_t *request,app_message_t *reply,pending_t **slot)
{
    if (!request || !reply || request == reply || !endpoint_valid(request->source) || !endpoint_valid(request->target) ||
        request->source == request->target || !request->generation || request->deadline_us <= 0) {
        app_message_release(request); return ESP_ERR_INVALID_ARG;
    }
    memset(reply, 0, sizeof(*reply));
    if (!mutex) { app_message_release(request); return ESP_ERR_INVALID_STATE; }
    if (xSemaphoreTake(mutex, deadline_ticks(request->deadline_us)) != pdTRUE) {
        app_message_release(request); return ESP_ERR_TIMEOUT;
    }
    pending_t *p = NULL;
    esp_err_t admission = ESP_OK;
    if (!status.accepting || !endpoints[request->source].active || !endpoints[request->target].active)
        admission = ESP_ERR_INVALID_STATE;
    else if (expired(request)) admission = ESP_ERR_TIMEOUT;
    if (admission == ESP_OK)
        for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i) if (!pending[i].used) { p = &pending[i]; break; }
    if (!p) {
        xSemaphoreGive(mutex); app_message_release(request);
        return admission == ESP_OK ? ESP_ERR_NO_MEM : admission;
    }
    while (xSemaphoreTake(p->wake, 0) == pdTRUE) {}
    bool collision;
    do {
        if (++next_correlation == 0) ++next_correlation;
        collision = false;
        for (unsigned i = 0; i < APP_CONSOLE_PENDING_CAPACITY; ++i)
            if (pending[i].used && pending[i].correlation == next_correlation) collision = true;
    } while (collision);
    p->used = true; p->complete = false; p->result = ESP_ERR_TIMEOUT;
    p->source = request->source; p->target = request->target; p->type = request->type;
    p->correlation = next_correlation; p->generation = request->generation; p->deadline = request->deadline_us;
    p->source_epoch = endpoints[p->source].epoch; p->target_epoch = endpoints[p->target].epoch;
    memset(&p->reply, 0, sizeof(p->reply)); ++status.pending;
    request->correlation_id = p->correlation;
    request->flags = (request->flags & APP_MESSAGE_BULK) | APP_MESSAGE_REQUEST;
    xSemaphoreGive(mutex);
    *slot=p;return ESP_OK;
}
static esp_err_t finish_request(pending_t *p,app_message_t *reply,esp_err_t error,
    app_console_cancel_fn cancelled,void *context)
{
    if (error == ESP_OK && !cancelled) xSemaphoreTake(p->wake, deadline_ticks(p->deadline));
    else if (error == ESP_OK) {
        for (;;) {
            if (cancelled(context)) { error = ESP_ERR_INVALID_STATE; break; }
            TickType_t remaining = deadline_ticks(p->deadline);
            if (!remaining) break;
            TickType_t slice = pdMS_TO_TICKS(25);
            if (!slice) slice = 1;
            if (slice > remaining) slice = remaining;
            if (xSemaphoreTake(p->wake, slice) == pdTRUE) break;
        }
    }
    xSemaphoreTake(mutex, portMAX_DELAY);
    if (error == ESP_OK) error = p->complete ? p->result : ESP_ERR_TIMEOUT;
    *reply = p->reply; p->reply.lease = NULL;
    p->used = false; --status.pending;
    xSemaphoreGive(mutex);
    if (error != ESP_OK) app_message_release(reply);
    return error;
}
esp_err_t app_console_request_cancelable(app_message_t *request, app_message_t *reply,
    app_console_cancel_fn cancelled, void *context)
{
    pending_t *slot=NULL;
    esp_err_t error=begin_request(request,reply,&slot);
    if (error!=ESP_OK) return error;
    /* Blocking RPC must not turn a momentary SMP lock collision into a
     * transport timeout. Queue admission stays nonblocking; only the router
     * lock waits, within the original absolute request deadline. */
    error=send_with_lock_wait(request,deadline_ticks(request->deadline_us));
    return finish_request(slot,reply,error,cancelled,context);
}
esp_err_t app_console_request_many(app_message_t *requests,app_message_t *replies,
    esp_err_t *results,size_t count)
{
    if (!requests || !replies || !results || !count || count>APP_CONSOLE_PENDING_CAPACITY || requests==replies)
        return ESP_ERR_INVALID_ARG;
    pending_t *slots[APP_CONSOLE_PENDING_CAPACITY]={0};
    /* All sends precede the first wait: independent endpoint work overlaps.
     * Each request keeps its own absolute deadline and ownership reservation. */
    for (size_t i=0;i<count;++i) {
        memset(&replies[i],0,sizeof(replies[i]));
        results[i]=begin_request(&requests[i],&replies[i],&slots[i]);
        if (results[i]==ESP_OK)
            results[i]=send_with_lock_wait(&requests[i],deadline_ticks(requests[i].deadline_us));
    }
    for (size_t i=0;i<count;++i) if (slots[i])
        results[i]=finish_request(slots[i],&replies[i],results[i],NULL,NULL);
    return ESP_OK;
}
esp_err_t app_console_request(app_message_t *request, app_message_t *reply)
{ return app_console_request_cancelable(request, reply, NULL, NULL); }

esp_err_t app_console_reply(const app_message_t *request, app_message_t *reply)
{
    if (!request || !reply || !(request->flags & APP_MESSAGE_REQUEST) || !request->correlation_id) {
        app_message_release(reply); return ESP_ERR_INVALID_ARG;
    }
    app_message_t envelope = *request;
    reply->type = envelope.type; reply->source = envelope.target; reply->target = envelope.source;
    reply->correlation_id = envelope.correlation_id; reply->generation = envelope.generation;
    reply->deadline_us = envelope.deadline_us; reply->flags = APP_MESSAGE_REPLY;
    return send_with_lock_wait(reply,deadline_ticks(reply->deadline_us));
}

bool app_console_router_quiesce(uint32_t timeout_ms)
{
    if (!mutex) return true;
    xSemaphoreTake(mutex, portMAX_DELAY);
    status.accepting = false; cancel_pending(APP_ENDPOINT_NONE, ESP_ERR_INVALID_STATE);
    xSemaphoreGive(mutex);
    for (unsigned i = 1; i < APP_ENDPOINT_COUNT; ++i) app_console_endpoint_stop((app_endpoint_t)i);
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    for (;;) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        bool drained = !status.pending && !app_message_lease_count();
        if (drained) status.running = false;
        xSemaphoreGive(mutex);
        if (drained && !atomic_load(&housekeeping_running)) return true;
        if (esp_timer_get_time() >= deadline) return false;
        vTaskDelay(1);
    }
}

void app_console_get_status(app_console_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!mutex) return;
    xSemaphoreTake(mutex, portMAX_DELAY); *out = status;
    out->leases = app_message_lease_count(); xSemaphoreGive(mutex);
}
