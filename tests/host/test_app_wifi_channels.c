#include "app_wifi_driver.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static unsigned generation = 1, opened, closed, destroyed, calls;
static bool online = true, close_during_io;
static app_wifi_result_t stop_result;
static app_wifi_channel_t *channel;
static const void *buffer;
static app_wifi_result_t start(void *context, const network_config_t *config)
{ (void)context; (void)config; online = true; return APP_WIFI_OK; }
static app_wifi_result_t stop(void *context, uint32_t timeout)
{ (void)context; (void)timeout; if (stop_result == APP_WIFI_OK) { online = false; ++generation; } return stop_result; }
static app_wifi_result_t status(void *context, app_wifi_status_t *out)
{ (void)context; *out = (app_wifi_status_t){.online = online, .generation = generation, .max_channel = 13}; return APP_WIFI_OK; }
static app_wifi_result_t clients(void *context, app_wifi_client_t *out, size_t capacity, size_t *count)
{ (void)context; (void)out; (void)capacity; *count = 0; return APP_WIFI_OK; }
static void destroy(void *context) { (void)context; ++destroyed; }
static app_wifi_result_t open(void *context, const app_wifi_endpoint_t *endpoint, const app_wifi_deadline_t *deadline, void **out)
{
    (void)context; assert(endpoint->port == 15740 && deadline->generation == generation);
    *out = malloc(1); ++opened; return APP_WIFI_OK;
}
static app_wifi_result_t io(void *context, void *data, size_t size, bool transmit, const app_wifi_deadline_t *deadline, size_t *bytes)
{
    assert(context && data == buffer && !transmit); ++calls;
    if (close_during_io) { assert(app_wifi_channel_close(&channel) == APP_WIFI_STATE && channel); }
    if (deadline->cancelled(deadline->context)) { *bytes = 0; return APP_WIFI_CANCELLED; }
    *bytes = size; return APP_WIFI_OK;
}
static void close_context(void *context) { free(context); ++closed; }
int main(void)
{
    int context; app_wifi_t *wifi = NULL;
    app_wifi_driver_ops_t ops = {.api_version = APP_WIFI_API_VERSION, .capabilities = APP_WIFI_CAP_AP | APP_WIFI_CAP_TCP,
        .start = start, .reconfigure = start, .stop = stop, .status = status, .clients = clients, .destroy = destroy};
    assert(app_wifi_driver_bind(&ops, &context, &wifi) == APP_WIFI_INVALID);
    ops.channel_open = open; ops.channel_io = io; ops.channel_close = close_context;
    assert(app_wifi_driver_bind(&ops, &context, &wifi) == APP_WIFI_OK);
    app_wifi_endpoint_t endpoint = {.address = "192.168.4.9", .port = 15740};
    app_wifi_deadline_t deadline = {.at_us = 1000000};
    assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &channel) == APP_WIFI_STATE && !opened);
    network_config_t config; network_config_make_default(&config); assert(app_wifi_start(wifi, &config) == APP_WIFI_OK);
    deadline.generation = 2; assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &channel) == APP_WIFI_STALE);
    deadline.generation = 0; assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &channel) == APP_WIFI_OK);
    assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &channel) == APP_WIFI_INVALID && opened == 1);
    uint8_t data[9]; buffer = data; size_t bytes = 9;
    bool readable = true;
    assert(app_wifi_channel_poll(channel, &deadline, &readable) == APP_WIFI_UNSUPPORTED && !readable);
    assert(app_wifi_channel_receive(channel, data, 9, &deadline, &bytes) == APP_WIFI_OK && bytes == 9 && calls == 1);
    close_during_io = true;
    assert(app_wifi_channel_receive(channel, data, 9, &deadline, &bytes) == APP_WIFI_CANCELLED && !bytes && !closed);
    assert(app_wifi_channel_close(&channel) == APP_WIFI_OK && !channel && closed == 1);
    assert(app_wifi_channel_close(&channel) == APP_WIFI_OK && closed == 1); close_during_io = false;
    assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &channel) == APP_WIFI_OK);
    stop_result = APP_WIFI_TIMEOUT; assert(app_wifi_stop(wifi, 100) == APP_WIFI_TIMEOUT);
    app_wifi_channel_t *other = NULL;
    assert(app_wifi_channel_connect(wifi, &endpoint, &deadline, &other) == APP_WIFI_STATE);
    assert(app_wifi_channel_receive(channel, data, 9, &deadline, &bytes) == APP_WIFI_CANCELLED);
    stop_result = APP_WIFI_OK; assert(app_wifi_stop(wifi, 100) == APP_WIFI_OK);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_STATE && !destroyed);
    assert(app_wifi_channel_receive(channel, data, 9, &deadline, &bytes) == APP_WIFI_STALE);
    assert(app_wifi_channel_close(&channel) == APP_WIFI_OK && closed == 2);
    assert(app_wifi_destroy(&wifi) == APP_WIFI_OK && !wifi && destroyed == 1);
    return 0;
}
