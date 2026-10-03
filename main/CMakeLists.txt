idf_component_register(SRCS "app_main.c" "wifi_ap.c" "wifi_config.c" "wifi_apply.c" "factory_reset.c" "wifi_menu.c" "wifi_menu_ui.c" "camera_controller.c" "camera_menu.c" "camera_link.c" "camera_identity.c" "camera_console.c" "wifi_console.c" "liveview_pipeline.c" "atom_link.c" "gamepad_input.c" "camera_actions.c" "setting_control.c" "../common/debug_args.c" "../common/debug_line.c" "../common/debug_console.c" "../common/atom_protocol.c" "../common/atom_client.c" INCLUDE_DIRS "." "../common"
    PRIV_REQUIRES esp_new_jpeg ptpip sony_camera board_7b esp_psram esp_timer nvs_flash esp_wifi esp_netif esp_event lwip esp_driver_uart esp_driver_i2c esp_app_format esp_http_server app_update json)
target_sources(${COMPONENT_LIB} PRIVATE "maint_mode.c" "maint_auth.c" "maint_web.c" "maint_probe.c" "maint_json.c" "maint_wifi.c")
target_sources(${COMPONENT_LIB} PRIVATE "app_restart.c" "display_bench.c")
target_sources(${COMPONENT_LIB} PRIVATE "maint_ota.c" "ota_header.c")
execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../tools/pack_web.py"
    "${CMAKE_CURRENT_LIST_DIR}/web/index.html" "${CMAKE_CURRENT_BINARY_DIR}/index.html.gz"
    RESULT_VARIABLE web_pack_result)
if(NOT web_pack_result EQUAL 0)
    message(FATAL_ERROR "Cannot compress maintenance page")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "web/index.html" "../tools/pack_web.py")
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/index.html.gz" BINARY)
target_sources(${COMPONENT_LIB} PRIVATE "../common/i2c_monitor.c" "../common/i2c_debug.c")
target_sources(${COMPONENT_LIB} PRIVATE "lcd_sim.c")
target_sources(${COMPONENT_LIB} PRIVATE "ui_preferences.c")
if(CONFIG_REMOTE_DBG_SIM)
    target_sources(${COMPONENT_LIB} PRIVATE "../common/atom_sim.c" "../common/pad_cmd.c" "../common/pad_player.c" "../m5_atom_matrix/main/ds4_events.c")
endif()
