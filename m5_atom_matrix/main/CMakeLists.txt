idf_component_register(SRCS "app_main.c" "ds4_host.c" "ds4_report.c" "ds4_events.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES driver esp_driver_gpio esp_driver_rmt bt nvs_flash)
