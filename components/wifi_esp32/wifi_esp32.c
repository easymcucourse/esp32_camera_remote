#include "wifi_esp32.h"
#include "app_wifi_driver.h"
#include "wifi_saved_config.h"
#include "wifi_config_jobs.h"
#include "wifi_tcp.h"
#include "sdkconfig.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    wifi_config_jobs_t *jobs;
    esp_netif_t *netif;
    esp_event_handler_instance_t events;
    SemaphoreHandle_t lifecycle, storage;
    portMUX_TYPE mux;
    bool driver, registered, own_event_loop, online;
    uint32_t generation;
    unsigned max_channel;
    esp_err_t diagnostic;
} wifi_esp32_t;
static atomic_bool claimed;
static app_wifi_result_t classify(esp_err_t error)
{
    if (error == ESP_OK) return APP_WIFI_OK;
    if (error == ESP_ERR_NO_MEM) return APP_WIFI_NO_MEMORY;
    if (error == ESP_ERR_INVALID_ARG) return APP_WIFI_INVALID;
    if (error == ESP_ERR_TIMEOUT) return APP_WIFI_TIMEOUT;
    return APP_WIFI_IO;
}
static void record(wifi_esp32_t *wifi, esp_err_t error)
{
    portENTER_CRITICAL(&wifi->mux); wifi->diagnostic = error; portEXIT_CRITICAL(&wifi->mux);
}
static void network_changed(wifi_esp32_t *wifi)
{
    portENTER_CRITICAL(&wifi->mux);
    if (++wifi->generation == 0) ++wifi->generation;
    wifi->online = false;
    portEXIT_CRITICAL(&wifi->mux);
}
static void event(void *context, esp_event_base_t base, int32_t id, void *data)
{
    (void)base; (void)data;
    wifi_esp32_t *wifi = context;
    if (id != WIFI_EVENT_AP_START && id != WIFI_EVENT_AP_STOP) return;
    portENTER_CRITICAL(&wifi->mux);
    wifi->online = id == WIFI_EVENT_AP_START;
    portEXIT_CRITICAL(&wifi->mux);
}
static esp_err_t configure(const network_config_t *settings)
{
    wifi_config_t config = {0};
    size_t ssid_size = strlen(settings->ssid), password_size = strlen(settings->password);
    memcpy(config.ap.ssid, settings->ssid, ssid_size);
    memcpy(config.ap.password, settings->password, password_size);
    config.ap.ssid_len = ssid_size; config.ap.channel = settings->channel;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK; config.ap.max_connection = APP_WIFI_CLIENT_CAPACITY;
    config.ap.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_AP, &config);
}
static app_wifi_result_t start(void *context, const network_config_t *config)
{
    wifi_esp32_t *wifi = context;
    if (xSemaphoreTake(wifi->lifecycle, 0) != pdTRUE) return APP_WIFI_STATE;
    network_changed(wifi);
    esp_err_t error = configure(config);
    if (error == ESP_OK) error = esp_wifi_start();
    if (error == ESP_OK) {
        wifi_jobs_set(wifi->jobs, config, wifi->max_channel);
        if (wifi_jobs_start(wifi->jobs) != APP_WIFI_OK) {
            esp_wifi_stop(); error = ESP_ERR_NO_MEM;
        }
    }
    record(wifi, error); xSemaphoreGive(wifi->lifecycle); return classify(error);
}
static app_wifi_result_t reconfigure(void *context, const network_config_t *config)
{
    wifi_esp32_t *wifi = context;
    if (xSemaphoreTake(wifi->lifecycle, 0) != pdTRUE) return APP_WIFI_STATE;
    network_changed(wifi);
    esp_err_t error = esp_wifi_stop();
    if (error == ESP_OK) error = configure(config);
    if (error == ESP_OK) error = esp_wifi_start();
    record(wifi, error); xSemaphoreGive(wifi->lifecycle); return classify(error);
}
static app_wifi_result_t stop(void *context, uint32_t timeout_ms)
{
    wifi_esp32_t *wifi = context;
    app_wifi_result_t drained = wifi_jobs_stop(wifi->jobs, timeout_ms);
    if (drained != APP_WIFI_OK) return drained;
    /* Timeout bounds admission; ESP-IDF stop is synchronous and completes before
     * releasing lifecycle ownership. No freed driver can race an admitted call. */
    if (xSemaphoreTake(wifi->lifecycle, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return APP_WIFI_TIMEOUT;
    network_changed(wifi);
    esp_err_t error = esp_wifi_stop();
    record(wifi, error); xSemaphoreGive(wifi->lifecycle); return classify(error);
}
static app_wifi_result_t status(void *context, app_wifi_status_t *out)
{
    wifi_esp32_t *wifi = context;
    *out = (app_wifi_status_t){0};
    portENTER_CRITICAL(&wifi->mux);
    out->online = wifi->online; out->generation = wifi->generation;
    out->max_channel = wifi->max_channel; out->diagnostic = wifi->diagnostic;
    portEXIT_CRITICAL(&wifi->mux);
    esp_netif_ip_info_t ip;
    if (out->online && esp_netif_is_netif_up(wifi->netif) && esp_netif_get_ip_info(wifi->netif, &ip) == ESP_OK)
        snprintf(out->address, sizeof(out->address), IPSTR, IP2STR(&ip.ip));
    return APP_WIFI_OK;
}
static app_wifi_result_t clients(void *context, app_wifi_client_t *out, size_t capacity, size_t *count)
{
    wifi_esp32_t *wifi = context;
    *count = 0;
    wifi_sta_list_t peers = {0};
    esp_err_t error = esp_wifi_ap_get_sta_list(&peers);
    if (error != ESP_OK) { record(wifi, error); return classify(error); }
    if (peers.num > ESP_WIFI_MAX_CONN_NUM) return APP_WIFI_IO;
    if (!peers.num) return APP_WIFI_OK;
    esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM] = {0};
    for (unsigned i = 0; i < peers.num; ++i) memcpy(pairs[i].mac, peers.sta[i].mac, 6);
    error = esp_netif_dhcps_get_clients_by_mac(wifi->netif, peers.num, pairs);
    if (error != ESP_OK) { record(wifi, error); return classify(error); }
    for (unsigned i = 0; i < peers.num; ++i) if (pairs[i].ip.addr) {
        if (*count >= capacity) { *count = 0; return APP_WIFI_NO_MEMORY; }
        app_wifi_client_t *client = &out[(*count)++];
        memcpy(client->mac, peers.sta[i].mac, 6);
        snprintf(client->ip, sizeof(client->ip), IPSTR, IP2STR(&pairs[i].ip));
        client->rssi = peers.sta[i].rssi;
    }
    return APP_WIFI_OK;
}
static void radio_cleanup(wifi_esp32_t *wifi)
{
    if (wifi->registered) esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi->events);
    if (wifi->driver) esp_wifi_deinit();
    if (wifi->netif) esp_netif_destroy_default_wifi(wifi->netif);
    if (wifi->own_event_loop) esp_event_loop_delete_default();
    wifi->registered = false; wifi->driver = false; wifi->netif = NULL;
    wifi->own_event_loop = false;
}
static void destroy(void *context)
{
    wifi_esp32_t *wifi = context;
    radio_cleanup(wifi);
    wifi_jobs_destroy(wifi->jobs);
    if (wifi->storage) vSemaphoreDelete(wifi->storage);
    if (wifi->lifecycle) vSemaphoreDelete(wifi->lifecycle);
    free(wifi); atomic_store(&claimed, false);
}
static app_wifi_result_t initialize(void *context)
{
    wifi_esp32_t *wifi = context;
    esp_err_t error = esp_netif_init();
    if (error == ESP_OK) {
        error = esp_event_loop_create_default();
        if (error == ESP_ERR_INVALID_STATE) error = ESP_OK;
        else if (error == ESP_OK) wifi->own_event_loop = true;
    }
    if (error == ESP_OK) {
        wifi->netif = esp_netif_create_default_wifi_ap();
        if (!wifi->netif) error = ESP_ERR_NO_MEM;
    }
    if (error == ESP_OK) {
        wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); init.nvs_enable = false;
        error = esp_wifi_init(&init); wifi->driver = error == ESP_OK;
    }
    if (error == ESP_OK) error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error == ESP_OK) error = esp_wifi_set_country_code(CONFIG_APP_WIFI_COUNTRY, true);
    if (error == ESP_OK) {
        wifi_country_t country; error = esp_wifi_get_country(&country);
        if (error == ESP_OK) {
            unsigned limit = country.schan + country.nchan - 1;
            portENTER_CRITICAL(&wifi->mux);
            wifi->max_channel = limit > 13 ? 13 : limit;
            portEXIT_CRITICAL(&wifi->mux);
        }
    }
    if (error == ESP_OK) error = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event, wifi, &wifi->events);
    wifi->registered = error == ESP_OK;
    if (error == ESP_OK) error = esp_wifi_set_mode(WIFI_MODE_AP);
    if (error != ESP_OK) { radio_cleanup(wifi); record(wifi, error); return classify(error); }
    return APP_WIFI_OK;
}
static app_wifi_result_t saved_read(void *context, network_config_t *config)
{
    wifi_esp32_t *wifi = context;
    if (xSemaphoreTake(wifi->storage, 0) != pdTRUE) return APP_WIFI_STATE;
    esp_err_t error = wifi_saved_read(config);
    if (error == ESP_OK && !wifi->driver) wifi_jobs_set(wifi->jobs, config, wifi->max_channel);
    record(wifi, error); xSemaphoreGive(wifi->storage); return classify(error);
}
static app_wifi_result_t saved_write(void *context, const network_config_t *config)
{
    wifi_esp32_t *wifi = context;
    if (xSemaphoreTake(wifi->storage, 0) != pdTRUE) return APP_WIFI_STATE;
    esp_err_t error = wifi_saved_write(config);
    record(wifi, error); xSemaphoreGive(wifi->storage); return classify(error);
}
static app_wifi_result_t config_get(void *c, network_config_t *out)
{ return wifi_jobs_get(((wifi_esp32_t *)c)->jobs, out); }
static app_wifi_result_t config_apply(void *c, const network_config_t *config, bool staged, uint32_t *token)
{ return wifi_jobs_apply(((wifi_esp32_t *)c)->jobs, config, staged, token); }
static app_wifi_result_t config_commit(void *c, uint32_t token, unsigned delay_ms)
{ return wifi_jobs_commit(((wifi_esp32_t *)c)->jobs, token, delay_ms); }
static app_wifi_result_t config_cancel(void *c, uint32_t token)
{ return wifi_jobs_cancel(((wifi_esp32_t *)c)->jobs, token); }
static app_wifi_result_t config_result(void *c, uint32_t token, app_wifi_result_t *result)
{ return wifi_jobs_result(((wifi_esp32_t *)c)->jobs, token, result); }
static app_wifi_result_t config_freeze(void *c, uint32_t timeout_ms)
{ return wifi_jobs_freeze(((wifi_esp32_t *)c)->jobs, timeout_ms); }
static void config_resume(void *c) { wifi_jobs_resume(((wifi_esp32_t *)c)->jobs); }
static app_wifi_result_t config_start(void *context)
{ return wifi_jobs_start(((wifi_esp32_t *)context)->jobs); }
static app_wifi_result_t config_quiesce(void *c, uint32_t timeout_ms)
{ return wifi_jobs_stop(((wifi_esp32_t *)c)->jobs, timeout_ms); }
static void tcp_network(void *context, uint32_t *generation, bool *online)
{
    wifi_esp32_t *wifi = context;
    portENTER_CRITICAL(&wifi->mux); *generation = wifi->generation; *online = wifi->online; portEXIT_CRITICAL(&wifi->mux);
}
static void tcp_diagnostic(void *context, int error) { record(context, -error); }
static app_wifi_result_t channel_open(void *context, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, void **channel)
{ return wifi_tcp_open(context, tcp_network, tcp_diagnostic, endpoint, deadline, channel); }
static const app_wifi_driver_ops_t ops = {
    .api_version = APP_WIFI_API_VERSION,
    .capabilities = APP_WIFI_CAP_AP | APP_WIFI_CAP_CONFIG_STORE | APP_WIFI_CAP_CONFIG_ASYNC | APP_WIFI_CAP_TCP,
    .start = start, .reconfigure = reconfigure, .stop = stop, .status = status,
    .clients = clients, .destroy = destroy, .init = initialize,
    .saved_read = saved_read, .saved_write = saved_write,
    .config_get = config_get, .config_apply = config_apply, .config_commit = config_commit,
    .config_cancel = config_cancel, .config_result = config_result,
    .config_freeze = config_freeze, .config_resume = config_resume,
    .channel_open = channel_open, .channel_io = wifi_tcp_io, .channel_close = wifi_tcp_close,
    .channel_poll = wifi_tcp_poll,
    .config_quiesce = config_quiesce,.config_start = config_start,
};
app_wifi_result_t wifi_esp32_create(app_wifi_t **out)
{
    if (!out || *out) return APP_WIFI_INVALID;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&claimed, &expected, true)) return APP_WIFI_STATE;
    wifi_esp32_t *wifi = calloc(1, sizeof(*wifi));
    if (!wifi) { atomic_store(&claimed, false); return APP_WIFI_NO_MEMORY; }
    wifi->mux = (portMUX_TYPE)portMUX_INITIALIZER_UNLOCKED; wifi->max_channel = 13;
    wifi->lifecycle = xSemaphoreCreateMutex();
    wifi->storage = xSemaphoreCreateMutex();
    if (!wifi->lifecycle || !wifi->storage) { destroy(wifi); return APP_WIFI_NO_MEMORY; }
    wifi->jobs = wifi_jobs_create(wifi, saved_write, reconfigure);
    if (!wifi->jobs) { destroy(wifi); return APP_WIFI_NO_MEMORY; }
    app_wifi_result_t result = app_wifi_driver_bind(&ops, wifi, out);
    if (result != APP_WIFI_OK) destroy(wifi);
    return result;
}
