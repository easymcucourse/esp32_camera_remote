idf_component_register(SRCS "app_message.c" "app_message_router.c" "app_console_uart.c" "wifi_console.c" "camera_commands.c" "uart_status.c" "i2c_console.c" "ui_preferences_console.c" "../../common/debug_args.c" "../../common/debug_line.c" "../../common/debug_console.c"
    INCLUDE_DIRS "include" "../../common" PRIV_INCLUDE_DIRS "private"
    PRIV_REQUIRES common_runtime freertos esp_timer esp_driver_uart esp_app_format esp_hw_support app_update)

if(CONFIG_REMOTE_DBG_SIM)
    target_sources(${COMPONENT_LIB} PRIVATE "display_bench.c" "lcd_sim.c" "../../common/pad_cmd.c")
endif()
