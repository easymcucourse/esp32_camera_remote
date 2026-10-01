idf_component_register(SRCS "app_main.c" "wifi_ap.c" "camera_controller.c" "camera_link.c" "camera_identity.c" "camera_console.c" "liveview_pipeline.c" "atom_link.c" "focus_input.c" INCLUDE_DIRS "."
    PRIV_REQUIRES ptpip sony_camera board_7b esp_psram esp_timer nvs_flash esp_wifi esp_netif esp_event lwip esp_driver_uart esp_driver_i2c)
