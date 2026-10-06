idf_component_register(SRCS "gamepad_input.c" "input_reports.c" "input_provider.c" "input_owner.c" "input_service.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private" "../../common"
    REQUIRES app_console PRIV_REQUIRES freertos esp_timer)
