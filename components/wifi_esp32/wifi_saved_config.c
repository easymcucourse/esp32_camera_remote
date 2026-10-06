#include "wifi_saved_config.h"
#include "nvs.h"
#include "esp_log.h"

esp_err_t wifi_saved_write(const network_config_t *config)
{
    uint8_t bytes[NETWORK_CONFIG_RECORD_SIZE];
    if (!network_config_encode(config, bytes)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t error = nvs_open("wifi_ap", NVS_READWRITE, &nvs);
    if (error != ESP_OK) return error;
    network_config_t defaults; network_config_make_default(&defaults);
    if (network_config_equal(config, &defaults)) {
        error = nvs_erase_key(nvs, "cfg");
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    } else error = nvs_set_blob(nvs, "cfg", bytes, sizeof(bytes));
    if (error == ESP_OK) error = nvs_commit(nvs);
    nvs_close(nvs); return error;
}
esp_err_t wifi_saved_read(network_config_t *config)
{
    if (!config) return ESP_ERR_INVALID_ARG;
    network_config_make_default(config);
    nvs_handle_t nvs;
    esp_err_t error = nvs_open("wifi_ap", NVS_READONLY, &nvs);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;
    uint8_t bytes[NETWORK_CONFIG_RECORD_SIZE]; size_t size = sizeof(bytes);
    error = nvs_get_blob(nvs, "cfg", bytes, &size);
    nvs_close(nvs);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error == ESP_OK && network_config_decode(bytes, size, config)) return ESP_OK;
    /* Oversized, malformed and unreadable records use the same existing repair
     * policy. A failed repair returns its diagnostic while keeping defaults. */
    network_config_make_default(config);
    ESP_LOGW("wifi_store", "Invalid Wi-Fi record (%s); restoring defaults", esp_err_to_name(error));
    return wifi_saved_write(config);
}
