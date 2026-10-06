#include "app_wifi_driver.h"
#include "async_token.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

struct app_wifi {
    const app_wifi_driver_ops_t *ops;
    void *context;
    atomic_bool started;
    atomic_bool accepting;
    atomic_uint channels;
    bool initialized; /* Composition owner only. */
};
static app_wifi_result_t check_config(app_wifi_t *wifi, const network_config_t *config);
app_wifi_result_t app_wifi_driver_bind(const app_wifi_driver_ops_t *ops,
    void *context, app_wifi_t **out)
{
    if (!out || *out || !context || !ops || ops->api_version != APP_WIFI_API_VERSION ||
        !(ops->capabilities & APP_WIFI_CAP_AP) || !ops->start || !ops->reconfigure ||
        !ops->stop || !ops->status || !ops->clients || !ops->destroy ||
        ((ops->capabilities & APP_WIFI_CAP_CONFIG_STORE) && (!ops->saved_read || !ops->saved_write)) ||
        ((ops->capabilities & APP_WIFI_CAP_CONFIG_ASYNC) && (!ops->config_get || !ops->config_apply ||
            !ops->config_commit || !ops->config_cancel || !ops->config_result || !ops->config_freeze || !ops->config_resume)) ||
        ((ops->capabilities & APP_WIFI_CAP_TCP) && (!ops->channel_open || !ops->channel_io || !ops->channel_close))) return APP_WIFI_INVALID;
    app_wifi_t *wifi = calloc(1, sizeof(*wifi));
    if (!wifi) return APP_WIFI_NO_MEMORY;
    wifi->ops = ops; wifi->context = context;
    atomic_init(&wifi->started, false);
    atomic_init(&wifi->accepting, false); atomic_init(&wifi->channels, 0);
    *out = wifi; return APP_WIFI_OK;
}
uint32_t app_wifi_next_token(void)
{
    return async_token_next();
}
static app_wifi_result_t config_available(app_wifi_t *wifi)
{
    if (!wifi) return APP_WIFI_INVALID;
    return wifi->ops->capabilities & APP_WIFI_CAP_CONFIG_ASYNC ? APP_WIFI_OK : APP_WIFI_UNSUPPORTED;
}
app_wifi_result_t app_wifi_config_get(app_wifi_t *wifi, network_config_t *config)
{
    if (!config) return APP_WIFI_INVALID;
    app_wifi_result_t error = config_available(wifi);
    return error == APP_WIFI_OK ? wifi->ops->config_get(wifi->context, config) : error;
}
app_wifi_result_t app_wifi_config_apply(app_wifi_t *wifi, const network_config_t *config, bool staged, uint32_t *token)
{
    if (!config || !token) return APP_WIFI_INVALID;
    app_wifi_result_t error = config_available(wifi);
    if (error != APP_WIFI_OK) return error;
    if (!atomic_load(&wifi->started)) return APP_WIFI_STATE;
    error = check_config(wifi, config);
    return error == APP_WIFI_OK ? wifi->ops->config_apply(wifi->context, config, staged, token) : error;
}
app_wifi_result_t app_wifi_config_commit(app_wifi_t *wifi, uint32_t token, unsigned delay_ms)
{
    if (!token || delay_ms > 10000) return APP_WIFI_INVALID;
    app_wifi_result_t error = config_available(wifi);
    return error == APP_WIFI_OK ? wifi->ops->config_commit(wifi->context, token, delay_ms) : error;
}
app_wifi_result_t app_wifi_config_cancel(app_wifi_t *wifi, uint32_t token)
{
    if (!token) return APP_WIFI_INVALID;
    app_wifi_result_t error = config_available(wifi);
    return error == APP_WIFI_OK ? wifi->ops->config_cancel(wifi->context, token) : error;
}
app_wifi_result_t app_wifi_config_result(app_wifi_t *wifi, uint32_t token, app_wifi_result_t *result)
{
    if (!token || !result) return APP_WIFI_INVALID;
    app_wifi_result_t error = config_available(wifi);
    return error == APP_WIFI_OK ? wifi->ops->config_result(wifi->context, token, result) : error;
}
app_wifi_result_t app_wifi_config_freeze(app_wifi_t *wifi, uint32_t timeout_ms)
{
    app_wifi_result_t error = config_available(wifi);
    return error == APP_WIFI_OK ? wifi->ops->config_freeze(wifi->context, timeout_ms) : error;
}
void app_wifi_config_resume(app_wifi_t *wifi)
{ if (config_available(wifi) == APP_WIFI_OK) wifi->ops->config_resume(wifi->context); }
app_wifi_result_t app_wifi_config_quiesce(app_wifi_t *wifi, uint32_t timeout_ms)
{
    app_wifi_result_t error = config_available(wifi);
    if (error != APP_WIFI_OK) return error;
    return wifi->ops->config_quiesce ? wifi->ops->config_quiesce(wifi->context, timeout_ms) : APP_WIFI_UNSUPPORTED;
}
app_wifi_result_t app_wifi_config_start(app_wifi_t *wifi)
{
    app_wifi_result_t error=config_available(wifi);
    if(error!=APP_WIFI_OK)return error;
    if(!atomic_load(&wifi->started))return APP_WIFI_STATE;
    return wifi->ops->config_start ? wifi->ops->config_start(wifi->context) : APP_WIFI_UNSUPPORTED;
}
unsigned app_wifi_api_version(const app_wifi_t *wifi)
{ return wifi ? wifi->ops->api_version : 0; }
uint32_t app_wifi_capabilities(const app_wifi_t *wifi)
{ return wifi ? wifi->ops->capabilities : 0; }
app_wifi_result_t app_wifi_init(app_wifi_t *wifi)
{
    if (!wifi) return APP_WIFI_INVALID;
    if (wifi->initialized) return APP_WIFI_STATE;
    app_wifi_result_t error = wifi->ops->init ? wifi->ops->init(wifi->context) : APP_WIFI_OK;
    if (error == APP_WIFI_OK) wifi->initialized = true;
    return error;
}
app_wifi_result_t app_wifi_saved_config_read(app_wifi_t *wifi, network_config_t *config)
{
    if (!wifi || !config) return APP_WIFI_INVALID;
    if (!(wifi->ops->capabilities & APP_WIFI_CAP_CONFIG_STORE)) return APP_WIFI_UNSUPPORTED;
    network_config_t snapshot;
    app_wifi_result_t error = wifi->ops->saved_read(wifi->context, &snapshot);
    if (error != APP_WIFI_OK) return error;
    if (network_config_check(&snapshot, 13) != NETWORK_CFG_OK) return APP_WIFI_IO;
    *config = snapshot; return APP_WIFI_OK;
}
app_wifi_result_t app_wifi_saved_config_write(app_wifi_t *wifi, const network_config_t *config)
{
    if (!wifi || network_config_check(config, 13) != NETWORK_CFG_OK) return APP_WIFI_INVALID;
    if (!(wifi->ops->capabilities & APP_WIFI_CAP_CONFIG_STORE)) return APP_WIFI_UNSUPPORTED;
    return wifi->ops->saved_write(wifi->context, config);
}
static app_wifi_result_t check_config(app_wifi_t *wifi, const network_config_t *config)
{
    if (!wifi || !config) return APP_WIFI_INVALID;
    app_wifi_status_t status;
    app_wifi_result_t error = wifi->ops->status(wifi->context, &status);
    if (error != APP_WIFI_OK) return error;
    return network_config_check(config, status.max_channel) == NETWORK_CFG_OK ? APP_WIFI_OK : APP_WIFI_INVALID;
}
app_wifi_result_t app_wifi_start(app_wifi_t *wifi, const network_config_t *config)
{
    if (!wifi) return APP_WIFI_INVALID;
    if (atomic_load(&wifi->started)) return APP_WIFI_STATE;
    if (!wifi->initialized) {
        app_wifi_result_t initialized = app_wifi_init(wifi);
        if (initialized != APP_WIFI_OK) return initialized;
    }
    app_wifi_result_t error = check_config(wifi, config);
    if (error != APP_WIFI_OK) return error;
    error = wifi->ops->start(wifi->context, config);
    if (error == APP_WIFI_OK) { atomic_store(&wifi->started, true); atomic_store(&wifi->accepting, true); }
    return error;
}
app_wifi_result_t app_wifi_reconfigure(app_wifi_t *wifi, const network_config_t *config)
{
    if (!wifi) return APP_WIFI_INVALID;
    if (!atomic_load(&wifi->started)) return APP_WIFI_STATE;
    app_wifi_result_t error = check_config(wifi, config);
    return error == APP_WIFI_OK ? wifi->ops->reconfigure(wifi->context, config) : error;
}
app_wifi_result_t app_wifi_stop(app_wifi_t *wifi, uint32_t timeout_ms)
{
    if (!wifi) return APP_WIFI_INVALID;
    atomic_store(&wifi->accepting, false);
    if (!atomic_load(&wifi->started)) return APP_WIFI_OK;
    app_wifi_result_t error = wifi->ops->stop(wifi->context, timeout_ms);
    if (error == APP_WIFI_OK) atomic_store(&wifi->started, false);
    return error;
}
app_wifi_result_t app_wifi_destroy(app_wifi_t **wifi)
{
    if (!wifi || !*wifi) return APP_WIFI_INVALID;
    if (atomic_load(&(*wifi)->started) || atomic_load(&(*wifi)->channels)) return APP_WIFI_STATE;
    (*wifi)->ops->destroy((*wifi)->context); free(*wifi); *wifi = NULL;
    return APP_WIFI_OK;
}
struct app_wifi_channel {
    app_wifi_t *owner;
    void *context;
    uint32_t generation;
    atomic_bool busy, cancelled;
};
typedef struct { app_wifi_channel_t *channel; const app_wifi_deadline_t *original; } cancel_context_t;
static bool channel_cancelled(void *context)
{
    cancel_context_t *cancel = context;
    return atomic_load(&cancel->channel->cancelled) || !atomic_load(&cancel->channel->owner->accepting) ||
        (cancel->original->cancelled && cancel->original->cancelled(cancel->original->context));
}
app_wifi_result_t app_wifi_channel_connect(app_wifi_t *wifi, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, app_wifi_channel_t **out)
{
    if (!wifi || !endpoint || !out || *out || !deadline || deadline->at_us <= 0 ||
        !endpoint->port || !memchr(endpoint->address, 0, sizeof(endpoint->address))) return APP_WIFI_INVALID;
    if (!(wifi->ops->capabilities & APP_WIFI_CAP_TCP)) return APP_WIFI_UNSUPPORTED;
    atomic_fetch_add(&wifi->channels, 1); /* Pins owner during open and until close. */
    app_wifi_result_t result = APP_WIFI_STATE;
    if (!atomic_load(&wifi->accepting)) goto failed;
    app_wifi_status_t status;
    result = app_wifi_get_status(wifi, &status);
    if (result != APP_WIFI_OK) goto failed;
    if (!status.online || !status.generation) { result = APP_WIFI_STATE; goto failed; }
    if (deadline->generation && deadline->generation != status.generation) { result = APP_WIFI_STALE; goto failed; }
    app_wifi_channel_t *channel = calloc(1, sizeof(*channel));
    if (!channel) { result = APP_WIFI_NO_MEMORY; goto failed; }
    channel->owner = wifi; channel->generation = status.generation;
    atomic_init(&channel->busy, false); atomic_init(&channel->cancelled, false);
    app_wifi_deadline_t operation = *deadline; operation.generation = status.generation;
    result = wifi->ops->channel_open(wifi->context, endpoint, &operation, &channel->context);
    if (result == APP_WIFI_OK && !channel->context) result = APP_WIFI_IO;
    if (result == APP_WIFI_OK && !atomic_load(&wifi->accepting)) result = APP_WIFI_STATE;
    if (result == APP_WIFI_OK) {
        result = app_wifi_get_status(wifi, &status);
        if (result == APP_WIFI_OK && status.generation != channel->generation) result = APP_WIFI_STALE;
    }
    if (result != APP_WIFI_OK) {
        if (channel->context) wifi->ops->channel_close(channel->context);
        free(channel); goto failed;
    }
    *out = channel; return APP_WIFI_OK;
failed:
    atomic_fetch_sub(&wifi->channels, 1); return result;
}
static app_wifi_result_t channel_io(app_wifi_channel_t *channel, void *data, size_t size,
    bool transmit, const app_wifi_deadline_t *deadline, size_t *bytes, bool *readable)
{
    if (bytes) *bytes = 0;
    if (!channel || !bytes || (!data && size) || !deadline || deadline->at_us <= 0) return APP_WIFI_INVALID;
    if (deadline->generation && deadline->generation != channel->generation) return APP_WIFI_STALE;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&channel->busy, &expected, true)) return APP_WIFI_STATE;
    app_wifi_status_t status;
    app_wifi_result_t result = app_wifi_get_status(channel->owner, &status);
    if (result == APP_WIFI_OK && status.generation != channel->generation) result = APP_WIFI_STALE;
    if (result == APP_WIFI_OK && !atomic_load(&channel->owner->accepting)) result = APP_WIFI_CANCELLED;
    if (result == APP_WIFI_OK && (!status.started || !status.online)) result = APP_WIFI_STATE;
    if (result == APP_WIFI_OK) {
        cancel_context_t cancel = {channel, deadline};
        app_wifi_deadline_t operation = *deadline;
        operation.generation = channel->generation; operation.cancelled = channel_cancelled; operation.context = &cancel;
        if (readable) result = channel->owner->ops->channel_poll ?
            channel->owner->ops->channel_poll(channel->context, &operation, readable) : APP_WIFI_UNSUPPORTED;
        else {
            result = channel->owner->ops->channel_io(channel->context, data, size, transmit, &operation, bytes);
            if (*bytes > size) { *bytes = 0; result = APP_WIFI_IO; }
            else if (result == APP_WIFI_OK && *bytes != size) result = APP_WIFI_IO;
        }
    }
    atomic_store(&channel->busy, false); return result;
}
app_wifi_result_t app_wifi_channel_send(app_wifi_channel_t *channel, const void *data, size_t size,
    const app_wifi_deadline_t *deadline, size_t *bytes)
{ return channel_io(channel, (void *)data, size, true, deadline, bytes, NULL); }
app_wifi_result_t app_wifi_channel_receive(app_wifi_channel_t *channel, void *data, size_t size,
    const app_wifi_deadline_t *deadline, size_t *bytes)
{ return channel_io(channel, data, size, false, deadline, bytes, NULL); }
app_wifi_result_t app_wifi_channel_poll(app_wifi_channel_t *channel,
    const app_wifi_deadline_t *deadline, bool *readable)
{
    if (!readable) return APP_WIFI_INVALID;
    *readable = false; size_t bytes = 0;
    app_wifi_result_t result = channel_io(channel, NULL, 0, false, deadline, &bytes, readable);
    if (result != APP_WIFI_OK) *readable = false;
    return result;
}
void app_wifi_channel_cancel(app_wifi_channel_t *channel)
{ if (channel) atomic_store(&channel->cancelled, true); }
app_wifi_result_t app_wifi_channel_close(app_wifi_channel_t **channel)
{
    if (!channel) return APP_WIFI_INVALID;
    if (!*channel) return APP_WIFI_OK;
    app_wifi_channel_cancel(*channel);
    bool expected = false;
    if (!atomic_compare_exchange_strong(&(*channel)->busy, &expected, true)) return APP_WIFI_STATE;
    (*channel)->owner->ops->channel_close((*channel)->context);
    atomic_fetch_sub(&(*channel)->owner->channels, 1);
    free(*channel); *channel = NULL; return APP_WIFI_OK;
}
app_wifi_result_t app_wifi_get_status(app_wifi_t *wifi, app_wifi_status_t *status)
{
    if (!wifi || !status) return APP_WIFI_INVALID;
    memset(status, 0, sizeof(*status));
    app_wifi_result_t error = wifi->ops->status(wifi->context, status);
    status->started = atomic_load(&wifi->started);
    return error;
}
app_wifi_result_t app_wifi_get_clients(app_wifi_t *wifi, app_wifi_client_t *clients,
    size_t capacity, size_t *count)
{
    if (count) *count = 0;
    if (!wifi || !clients || !count || !capacity) return APP_WIFI_INVALID;
    if (!atomic_load(&wifi->started)) return APP_WIFI_STATE;
    app_wifi_client_t snapshot[APP_WIFI_CLIENT_CAPACITY]; size_t found = 0;
    app_wifi_result_t error = wifi->ops->clients(wifi->context, snapshot, APP_WIFI_CLIENT_CAPACITY, &found);
    if (error != APP_WIFI_OK) return error;
    if (found > APP_WIFI_CLIENT_CAPACITY) return APP_WIFI_IO;
    if (found > capacity) return APP_WIFI_NO_MEMORY;
    memcpy(clients, snapshot, found * sizeof(*clients)); *count = found;
    return APP_WIFI_OK;
}
