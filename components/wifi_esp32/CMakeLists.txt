idf_component_register(SRCS "wifi_esp32.c" "wifi_saved_config.c" "wifi_config_jobs.c" "wifi_tcp.c" "wifi_http_scope.c" INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private"
    REQUIRES app_wifi PRIV_REQUIRES common_runtime esp_wifi esp_netif esp_event esp_timer freertos nvs_flash lwip)
