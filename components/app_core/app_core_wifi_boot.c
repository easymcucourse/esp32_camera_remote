#include "app_core_wifi_boot.h"
#include "app_wifi.h"
#include "wifi_esp32.h"
#include "esp_log.h"


static const char *TAG = "wifi_ap";
static app_wifi_t *network;
void app_core_wifi_config(network_config_t *out)
{
    if (!out) return;
    if (app_wifi_config_get(network, out) != APP_WIFI_OK) network_config_make_default(out);
}
static unsigned max_channel(void)
{
    app_wifi_status_t status = {0}; app_wifi_get_status(network, &status);
    return status.max_channel ? status.max_channel : 13;
}
static esp_err_t convert(app_wifi_result_t error)
{
    switch (error) {
    case APP_WIFI_OK: return ESP_OK;
    case APP_WIFI_INVALID: return ESP_ERR_INVALID_ARG;
    case APP_WIFI_STATE: case APP_WIFI_CANCELLED: return ESP_ERR_INVALID_STATE;
    case APP_WIFI_NO_MEMORY: return ESP_ERR_NO_MEM;
    case APP_WIFI_TIMEOUT: return ESP_ERR_TIMEOUT;
    case APP_WIFI_PENDING: return ESP_ERR_NOT_FINISHED;
    case APP_WIFI_NOT_FOUND: return ESP_ERR_NOT_FOUND;
    case APP_WIFI_UNSUPPORTED: return ESP_ERR_NOT_SUPPORTED;
    default: return ESP_FAIL;
    }
}
esp_err_t app_core_wifi_create(void)
{
    app_wifi_result_t created=wifi_esp32_create(&network);
    if (created!=APP_WIFI_OK) return convert(created);
    network_config_t config;
    if (app_wifi_saved_config_read(network, &config) != APP_WIFI_OK)
        ESP_LOGW(TAG, "Wi-Fi record unavailable; defaults retained");
    return ESP_OK;
}
app_wifi_t *app_core_wifi_service(void) { return network; }
esp_err_t app_core_wifi_start(void)
{
    app_wifi_result_t initialized=app_wifi_init(network);
    if (initialized!=APP_WIFI_OK) return convert(initialized);
    network_config_t config; app_core_wifi_config(&config);
    if (network_config_check(&config, max_channel()) != NETWORK_CFG_OK) {
        network_config_make_default(&config);
    }
    app_wifi_result_t started=app_wifi_start(network, &config);
    if (started!=APP_WIFI_OK) return convert(started);
    return ESP_OK;
}
