#include "app_core.h"
#include "app_core_services.h"
#include "app_wifi.h"
#include "app_core_network.h"
#include "app_wifi_messages.h"
#include "esp_timer.h"

static app_wifi_t *network;
esp_err_t app_core_network_messages_start(void)
{ return network ? app_wifi_messages_start(network) : ESP_ERR_INVALID_STATE; }
esp_err_t app_core_network_bind(app_wifi_t *wifi)
{
    const uint32_t required = APP_WIFI_CAP_AP | APP_WIFI_CAP_CONFIG_ASYNC | APP_WIFI_CAP_CONFIG_STORE | APP_WIFI_CAP_TCP;
    if (!wifi || app_wifi_api_version(wifi) != APP_WIFI_API_VERSION ||
        (app_wifi_capabilities(wifi) & required) != required) return ESP_ERR_INVALID_ARG;
    if (network) return ESP_ERR_INVALID_STATE;
    network = wifi;
    return ESP_OK;
}
esp_err_t app_core_network_quiesce(uint32_t timeout_ms)
{
    if (!network) return ESP_ERR_INVALID_STATE;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    app_wifi_result_t error = app_wifi_config_quiesce(network, timeout_ms);
    if (error != APP_WIFI_OK) return error == APP_WIFI_TIMEOUT ? ESP_ERR_TIMEOUT :
        error == APP_WIFI_UNSUPPORTED ? ESP_ERR_NOT_SUPPORTED : ESP_ERR_INVALID_STATE;
    int64_t remaining = deadline - esp_timer_get_time();
    /* Stop only the normal message bridge. AP and saved storage belong to the
     * top-level Wi-Fi object and remain alive for the isolated Web app. */
    return app_wifi_messages_stop(remaining > 0 ? (uint32_t)(remaining / 1000) : 0);
}
