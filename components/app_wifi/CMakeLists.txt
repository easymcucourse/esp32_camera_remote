idf_component_register(SRCS "app_wifi.c" "wifi_apply.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "../../common"
    REQUIRES common_runtime)
