#include "app_wifi_driver.h"
#include <assert.h>
#include <string.h>

typedef struct {
    app_wifi_result_t start_result, change_result, stop_result, status_result, clients_result;
    unsigned starts, changes, stops, destroyed;
    unsigned initialized;
    app_wifi_result_t init_result;
    size_t count;
    app_wifi_status_t status;
} fake_t;
static app_wifi_result_t start(void *context, const network_config_t *config)
{ fake_t *f = context; assert(config); ++f->starts; return f->start_result; }
static app_wifi_result_t change(void *context, const network_config_t *config)
{ fake_t *f = context; assert(config); ++f->changes; ++f->status.generation; return f->change_result; }
static app_wifi_result_t stop(void *context, uint32_t timeout_ms)
{ fake_t *f = context; assert(timeout_ms == 100); ++f->stops; return f->stop_result; }
static app_wifi_result_t status(void *context, app_wifi_status_t *out)
{ fake_t *f = context; *out = f->status; return f->status_result; }
static app_wifi_result_t clients(void *context, app_wifi_client_t *out, size_t capacity, size_t *count)
{
    fake_t *f = context; assert(capacity == APP_WIFI_CLIENT_CAPACITY);
    for (size_t i = 0; i < capacity; ++i) out[i] = (app_wifi_client_t){.rssi = -40 - (int32_t)i};
    *count = f->count; return f->clients_result;
}
static void destroy(void *context) { ++((fake_t *)context)->destroyed; }
static app_wifi_result_t initialize(void *context)
{ fake_t *f = context; ++f->initialized; return f->init_result; }
int main(void)
{
    fake_t fake = {.count = 2, .status = {.max_channel = 11, .generation = 1}};
    app_wifi_driver_ops_t ops = {.api_version = APP_WIFI_API_VERSION, .capabilities = APP_WIFI_CAP_AP,
        .start = start, .reconfigure = change, .stop = stop, .status = status, .clients = clients, .destroy = destroy};
    app_wifi_t *wifi = NULL;
    ops.api_version++; assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_INVALID);
    assert(!wifi && !fake.destroyed); ops.api_version = APP_WIFI_API_VERSION;
    ops.capabilities = 0; assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_INVALID);
    ops.capabilities = APP_WIFI_CAP_AP; ops.clients = NULL;
    assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_INVALID); ops.clients = clients;
    ops.capabilities |= APP_WIFI_CAP_CONFIG_STORE;
    assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_INVALID);
    ops.capabilities = APP_WIFI_CAP_AP; ops.init = initialize;
    assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_OK);
    assert(app_wifi_driver_bind(&ops, &fake, &wifi) == APP_WIFI_INVALID);
    assert(app_wifi_api_version(wifi) == APP_WIFI_API_VERSION && app_wifi_capabilities(wifi) == APP_WIFI_CAP_AP);
    network_config_t config; network_config_make_default(&config);
    fake.init_result = APP_WIFI_IO;
    assert(app_wifi_start(wifi, &config) == APP_WIFI_IO && !fake.starts);
    fake.init_result = APP_WIFI_OK;
    assert(app_wifi_init(wifi) == APP_WIFI_OK && fake.initialized == 2);
    assert(app_wifi_init(wifi) == APP_WIFI_STATE);
    assert(app_wifi_saved_config_read(wifi, &config) == APP_WIFI_UNSUPPORTED);
    assert(app_wifi_saved_config_write(wifi, &config) == APP_WIFI_UNSUPPORTED);
    app_wifi_status_t snapshot;
    assert(app_wifi_get_status(wifi, &snapshot) == APP_WIFI_OK && !snapshot.started);
    assert(app_wifi_reconfigure(wifi, &config) == APP_WIFI_STATE);
    config.channel = 12; assert(app_wifi_start(wifi, &config) == APP_WIFI_INVALID && !fake.starts);
    config.channel = 6; fake.start_result = APP_WIFI_IO;
    assert(app_wifi_start(wifi, &config) == APP_WIFI_IO);
    assert(app_wifi_get_status(wifi, &snapshot) == APP_WIFI_OK && !snapshot.started);
    fake.start_result = APP_WIFI_OK; assert(app_wifi_start(wifi, &config) == APP_WIFI_OK);
    assert(app_wifi_start(wifi, &config) == APP_WIFI_STATE && fake.starts == 2);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_STATE && wifi && !fake.destroyed);
    app_wifi_client_t peers[4] = {{.rssi = 123}}; size_t count = 99;
    assert(app_wifi_get_clients(wifi, peers, 1, &count) == APP_WIFI_NO_MEMORY && !count && peers[0].rssi == 123);
    assert(app_wifi_get_clients(wifi, peers, 4, &count) == APP_WIFI_OK && count == 2 && peers[0].rssi == -40);
    fake.count = 5; assert(app_wifi_get_clients(wifi, peers, 4, &count) == APP_WIFI_IO && !count);
    fake.count = 2; fake.clients_result = APP_WIFI_TIMEOUT;
    assert(app_wifi_get_clients(wifi, peers, 4, &count) == APP_WIFI_TIMEOUT && !count);
    fake.status_result = APP_WIFI_IO;
    assert(app_wifi_reconfigure(wifi, &config) == APP_WIFI_IO && !fake.changes);
    fake.status_result = APP_WIFI_OK; fake.change_result = APP_WIFI_IO;
    assert(app_wifi_reconfigure(wifi, &config) == APP_WIFI_IO && fake.changes == 1);
    assert(app_wifi_get_status(wifi, &snapshot) == APP_WIFI_OK && snapshot.started && snapshot.generation == 2);
    fake.stop_result = APP_WIFI_TIMEOUT;
    assert(app_wifi_stop(wifi, 100) == APP_WIFI_TIMEOUT);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_STATE);
    fake.stop_result = APP_WIFI_OK; assert(app_wifi_stop(wifi, 100) == APP_WIFI_OK);
    assert(app_wifi_stop(wifi, 100) == APP_WIFI_OK && fake.stops == 2);
    count = 99; assert(app_wifi_get_clients(wifi, peers, 4, &count) == APP_WIFI_STATE && !count);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_OK && !wifi && fake.destroyed == 1);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_INVALID && !app_wifi_api_version(NULL));
    return 0;
}
