idf_component_register(SRCS "app_main.c" "wifi_ap.c" "camera_pair.c" INCLUDE_DIRS "."
    PRIV_REQUIRES board_7b esp_psram esp_timer nvs_flash esp_wifi esp_netif esp_event lwip esp_driver_uart)
