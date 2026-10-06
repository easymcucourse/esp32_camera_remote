#include "app_wifi_messages.h"
#include "app_wifi_driver.h"
#include "app_console.h"
#include "freertos/task.h"
#include <assert.h>
#include <string.h>

/* TCP worker service is independently exercised by wifi_channel_messages. */
esp_err_t wifi_channel_messages_start(app_wifi_t *wifi) { assert(wifi); return ESP_OK; }
esp_err_t wifi_channel_messages_dispatch(app_message_t *message) { (void)message; return ESP_ERR_NOT_SUPPORTED; }
void wifi_channel_messages_cancel(void) {}
bool wifi_channel_messages_idle(void) { return true; }
void wifi_channel_messages_cleanup(void) {}

static TaskFunction_t scheduled;
static void *task_context;
static bool registered, fail_task, fail_register;
static unsigned releases, replies, deletions;
static unsigned network_events;
static app_message_t queue[8], response[8];
static size_t used, position;
static int64_t now;
static app_wifi_result_t clients_error;
static void run(void) { TaskFunction_t fn = scheduled; scheduled = NULL; assert(fn); fn(task_context); }
BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack,
    void *context, unsigned priority, void *handle)
{
    (void)handle; assert(!strcmp(name, "wifi_endpoint") && stack == 4096 && priority == 2);
    if (fail_task) return pdFALSE;
    scheduled = fn; task_context = context; return pdPASS;
}
void vTaskDelete(void *task) { assert(!task); ++deletions; }
void vTaskDelay(TickType_t ticks) { now += (int64_t)ticks * 1000; if (scheduled) run(); }
int64_t esp_timer_get_time(void) { return now; }
esp_err_t app_console_endpoint_register(app_endpoint_t id, const app_endpoint_config_t *config)
{
    assert(id == APP_ENDPOINT_WIFI && config->control_depth == 16 && config->bulk_depth == 4);
    if (fail_register) return ESP_ERR_NO_MEM;
    registered = true; return ESP_OK;
}
void app_console_endpoint_stop(app_endpoint_t id) { assert(id == APP_ENDPOINT_WIFI); registered = false; }
esp_err_t app_console_receive(app_endpoint_t id, app_message_t *message, uint32_t timeout)
{
    assert(id == APP_ENDPOINT_WIFI && timeout == 50);
    if (!registered || position == used) return ESP_ERR_INVALID_STATE;
    now += 250000; /* Exercise the actual periodic event publisher. */
    *message = queue[position++]; return ESP_OK;
}
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->source==APP_ENDPOINT_WIFI && message->flags==APP_MESSAGE_EVENT && !message->lease);
    if (message->type==APP_MESSAGE_WIFI_NETWORK_CHANGED) {
        assert(message->generation==7 && message->payload.network.generation==7);
        ++network_events;
        /* Rejected notification must be retried; only successful publication
         * advances the producer's last-announced network generation. */
        if (network_events==1) return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}
esp_err_t app_console_reply(const app_message_t *request, app_message_t *reply)
{
    assert(request->flags == APP_MESSAGE_REQUEST && request->correlation_id == position);
    response[replies++] = *reply; return ESP_OK;
}
void app_message_release(app_message_t *message) { ++releases; message->lease = NULL; }
static app_wifi_result_t start(void *ctx, const network_config_t *config) { (void)ctx; (void)config; return APP_WIFI_OK; }
static app_wifi_result_t stop(void *ctx, uint32_t timeout) { (void)ctx; (void)timeout; return APP_WIFI_OK; }
static app_wifi_result_t status(void *ctx, app_wifi_status_t *out)
{ (void)ctx; *out = (app_wifi_status_t){.online = true, .max_channel = 13, .generation = 7}; strcpy(out->address, "192.168.4.1"); return APP_WIFI_OK; }
static app_wifi_result_t clients(void *ctx, app_wifi_client_t *out, size_t capacity, size_t *count)
{
    (void)ctx; assert(capacity == 4); *count = 1;
    out[0] = (app_wifi_client_t){.mac = {1,2,3,4,5,6}, .ip = "192.168.4.2", .rssi = -49};
    return clients_error;
}
static void destroy(void *ctx) { (void)ctx; }
static app_wifi_result_t config_get(void *ctx, network_config_t *out)
{ (void)ctx; network_config_make_default(out); return APP_WIFI_OK; }
static app_wifi_result_t config_apply(void *ctx, const network_config_t *config, bool staged, uint32_t *token)
{ (void)ctx; assert(staged && config->channel == 6); *token = 42; return APP_WIFI_OK; }
static app_wifi_result_t config_commit(void *ctx, uint32_t token, unsigned delay)
{ (void)ctx; assert(token == 42 && delay == 500); return APP_WIFI_OK; }
static app_wifi_result_t config_cancel(void *ctx, uint32_t token)
{ (void)ctx; assert(token == 42); return APP_WIFI_CANCELLED; }
static app_wifi_result_t config_result(void *ctx, uint32_t token, app_wifi_result_t *result)
{ (void)ctx; assert(token == 42); *result = APP_WIFI_IO; return APP_WIFI_OK; }
static app_wifi_result_t config_freeze(void *ctx, uint32_t timeout)
{ (void)ctx; (void)timeout; return APP_WIFI_OK; }
static void config_resume(void *ctx) { (void)ctx; }
static void add(app_message_type_t type)
{
    queue[used] = (app_message_t){.type = type, .source = APP_ENDPOINT_CAMERA,
        .target = APP_ENDPOINT_WIFI, .flags = APP_MESSAGE_REQUEST,
        .correlation_id = (uint32_t)used + 1}; ++used;
}
int main(void)
{
    const app_wifi_driver_ops_t ops = {.api_version = APP_WIFI_API_VERSION,
        .capabilities = APP_WIFI_CAP_AP | APP_WIFI_CAP_CONFIG_ASYNC,
        .start = start, .reconfigure = start, .stop = stop, .status = status, .clients = clients, .destroy = destroy,
        .config_get = config_get, .config_apply = config_apply, .config_commit = config_commit,
        .config_cancel = config_cancel, .config_result = config_result,
        .config_freeze = config_freeze, .config_resume = config_resume};
    int context; app_wifi_t *wifi = NULL;
    assert(app_wifi_driver_bind(&ops, &context, &wifi) == APP_WIFI_OK);
    network_config_t config; network_config_make_default(&config); assert(app_wifi_start(wifi, &config) == APP_WIFI_OK);
    assert(app_wifi_messages_start(NULL) == ESP_ERR_INVALID_ARG);
    fail_register = true; assert(app_wifi_messages_start(wifi) == ESP_ERR_NO_MEM); fail_register = false;
    fail_task = true; assert(app_wifi_messages_start(wifi) == ESP_ERR_NO_MEM && !registered); fail_task = false;
    assert(app_wifi_messages_start(wifi) == ESP_OK);
    assert(app_wifi_messages_start(wifi) == ESP_ERR_INVALID_STATE);
    add(APP_MESSAGE_WIFI_STATUS); add(APP_MESSAGE_CAMERA_DISCOVER); add(APP_MESSAGE_WIFI_RSSI);
    memcpy(queue[2].payload.peer.mac, (uint8_t[]){1,2,3,4,5,6}, 6);
    add(APP_MESSAGE_WIFI_RSSI); add(APP_MESSAGE_WIFI_CHANNEL_OPEN);
    run(); assert(replies == 5 && releases == 5 && deletions == 1);
    assert(network_events==2);
    assert(response[0].result == ESP_OK && response[0].payload.network.generation == 7 && response[0].payload.network.max_channel == 13);
    assert(!strcmp(response[0].payload.network.address, "192.168.4.1"));
    assert(response[1].payload.discovery.count == 1 && response[1].payload.discovery.clients[0].rssi == -49);
    assert(response[2].payload.peer.rssi == -49 && response[3].payload.peer.rssi == -127);
    assert(response[4].result == ESP_ERR_NOT_SUPPORTED);
    used = position = replies = 0;
    add(APP_MESSAGE_WIFI_CONFIG_GET); add(APP_MESSAGE_WIFI_CONFIG_PREPARE);
    strcpy(queue[1].payload.config.ssid, "host-test"); strcpy(queue[1].payload.config.password, "test-only-value");
    queue[1].payload.config.channel = 6;
    add(APP_MESSAGE_WIFI_CONFIG_COMMIT); queue[2].payload.command.token = 42; queue[2].payload.command.duration_ms = 500;
    add(APP_MESSAGE_WIFI_CONFIG_CANCEL); queue[3].payload.command.token = 42;
    add(APP_MESSAGE_WIFI_CONFIG_RESULT); queue[4].payload.command.token = 42;
    assert(app_wifi_messages_start(wifi) == ESP_OK); run();
    assert(response[0].result == ESP_OK && response[0].payload.config.channel == 6);
    for(unsigned i=1;i<5;++i) assert(response[i].result==ESP_ERR_NOT_SUPPORTED);
    used = position = replies = 0; clients_error = APP_WIFI_TIMEOUT; add(APP_MESSAGE_CAMERA_DISCOVER);
    assert(app_wifi_messages_start(wifi) == ESP_OK); run();
    assert(response[0].result == ESP_ERR_TIMEOUT && releases == 11);
    assert(app_wifi_messages_start(wifi) == ESP_OK);
    assert(app_wifi_messages_stop(0) == ESP_ERR_TIMEOUT);
    assert(app_wifi_messages_stop(100) == ESP_OK && !scheduled);
    assert(app_wifi_stop(wifi, 100) == APP_WIFI_OK && app_wifi_destroy(&wifi) == APP_WIFI_OK);
    return 0;
}
