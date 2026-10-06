idf_component_register(SRCS "app_wifi_messages.c" "wifi_channel_messages.c" INCLUDE_DIRS "include"
    REQUIRES app_wifi PRIV_REQUIRES common_runtime app_console freertos esp_timer)
