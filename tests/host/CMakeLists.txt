cmake_minimum_required(VERSION 3.16)
project(camera_parsers C)
enable_testing()
add_executable(test_gimbal test_gimbal.c ../../m5_atom_matrix/main/gimbal_control.c ../../m5_atom_matrix/main/gimbal_proto_rs3.c)
target_include_directories(test_gimbal PRIVATE ../../m5_atom_matrix/main)
target_compile_options(test_gimbal PRIVATE -Wall -Wextra -Werror)
add_test(NAME gimbal COMMAND test_gimbal)
add_executable(test_ble_clients test_ble_clients.c ../../m5_atom_matrix/main/ble_clients.c)
target_include_directories(test_ble_clients PRIVATE ble_client_stubs ../../m5_atom_matrix/main)
target_compile_options(test_ble_clients PRIVATE -Wall -Wextra -Werror)
add_test(NAME ble_clients COMMAND test_ble_clients)
add_executable(test_gimbal_tx test_gimbal_tx.c ../../m5_atom_matrix/main/gimbal_tx.c)
target_include_directories(test_gimbal_tx PRIVATE ../../m5_atom_matrix/main)
target_compile_options(test_gimbal_tx PRIVATE -Wall -Wextra -Werror)
add_test(NAME gimbal_tx COMMAND test_gimbal_tx)
foreach(ipv6 0 1)
    add_executable(test_wifi_http_scope_${ipv6} test_wifi_http_scope.c ../../components/wifi_esp32/wifi_http_scope.c)
    target_include_directories(test_wifi_http_scope_${ipv6} PRIVATE http_scope_stubs display_stubs ../../components/wifi_esp32/private)
    target_compile_definitions(test_wifi_http_scope_${ipv6} PRIVATE LWIP_IPV6=${ipv6})
    target_compile_options(test_wifi_http_scope_${ipv6} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME wifi_http_scope_${ipv6} COMMAND test_wifi_http_scope_${ipv6})
endforeach()
foreach(kind debug release)
    add_executable(test_core_start_${kind} test_core_start.c ../../components/app_core/app_core_start.c ../../components/app_core/app_core_mode.c ../../common/network_config.c)
    target_include_directories(test_core_start_${kind} PRIVATE core_start_stubs factory_stubs router_stubs display_stubs
        ../../components/app_core/include ../../components/app_core/private ../../components/app_camera/include
        ../../components/app_ui/include ../../components/app_wifi/include ../../components/app_console/include)
    target_include_directories(test_core_start_${kind} PRIVATE maintenance_stubs ../../components/app_maintenance/include)
    target_compile_definitions(test_core_start_${kind} PRIVATE CONFIG_REMOTE_DBG_SIM=$<IF:$<STREQUAL:${kind},debug>,1,0>)
    target_compile_options(test_core_start_${kind} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME core_start_${kind} COMMAND test_core_start_${kind} 0)
endforeach()
foreach(case N I P Q E V X K M G m g p a)
    add_test(NAME core_start_${case} COMMAND test_core_start_debug ${case})
endforeach()
add_executable(test_core_factory_messages test_core_factory_messages.c ../../components/app_core/app_core_messages.c)
target_include_directories(test_core_factory_messages PRIVATE camera_runtime_stubs display_stubs ../../components/app_core/include ../../components/app_core/private ../../components/app_ui/include ../../components/app_console/include ../../common)
target_compile_options(test_core_factory_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME core_factory_messages COMMAND test_core_factory_messages)
include_directories(../../common)
add_executable(test_ptpip_protocol test_ptpip_protocol.c ../../components/camera_backend_sony/sony_control_encoder.c ../../components/ptpip/ptpip_protocol.c
    ../../components/ptpip/ptpip_client.c ../../components/ptpip/ptp_wire.c
    ../../components/app_console/app_message.c)
target_include_directories(test_ptpip_protocol PRIVATE router_stubs display_stubs ../../common
    ../../components/ptpip/include ../../components/app_console/include ../../components/app_console/private)
target_include_directories(test_ptpip_protocol PRIVATE ../../components/camera_backend_sony/private)
target_compile_options(test_ptpip_protocol PRIVATE -Wall -Wextra -Werror)
target_sources(test_ptpip_protocol PRIVATE ../support/legacy/sony_exposure_encoder.c)
add_test(NAME ptpip_protocol COMMAND test_ptpip_protocol)
add_executable(test_ptpip_client test_ptpip_client.c ../../components/ptpip/ptpip_client.c
    ../../components/app_console/app_message.c)
target_include_directories(test_ptpip_client PRIVATE router_stubs display_stubs ../../common
    ../../components/ptpip/include ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_ptpip_client PRIVATE -Wall -Wextra -Werror)
add_test(NAME ptpip_client COMMAND test_ptpip_client)
add_executable(test_wifi_channel_messages test_wifi_channel_messages.c
    ../../components/app_wifi_messages/wifi_channel_messages.c ../../components/app_console/app_message.c
    ../../components/app_wifi/app_wifi.c ../../common/network_config.c)
target_include_directories(test_wifi_channel_messages PRIVATE router_stubs display_stubs ../../common
    ../../components/app_wifi_messages/private ../../components/app_wifi/include
    ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_wifi_channel_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_channel_messages COMMAND test_wifi_channel_messages)
add_executable(test_wifi_tcp test_wifi_tcp.c ../../components/wifi_esp32/wifi_tcp.c)
target_include_directories(test_wifi_tcp PRIVATE network_stubs display_stubs
    ../../components/wifi_esp32/private ../../components/app_wifi/include)
target_compile_definitions(test_wifi_tcp PRIVATE fcntl=net_fcntl)
target_compile_options(test_wifi_tcp PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_tcp COMMAND test_wifi_tcp)
add_executable(test_app_wifi_channels test_app_wifi_channels.c ../../components/app_wifi/app_wifi.c ../../common/network_config.c)
target_include_directories(test_app_wifi_channels PRIVATE ../../components/app_wifi/include)
target_compile_options(test_app_wifi_channels PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_wifi_channels COMMAND test_app_wifi_channels)
add_executable(test_wifi_config_jobs test_wifi_config_jobs.c
    ../../components/wifi_esp32/wifi_config_jobs.c ../../components/app_wifi/app_wifi.c
    ../../common/network_config.c ../../components/app_wifi/wifi_apply.c)
target_include_directories(test_wifi_config_jobs PRIVATE router_stubs display_stubs
    ../../components/wifi_esp32/private ../../components/app_wifi/include)
target_compile_options(test_wifi_config_jobs PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_config_jobs COMMAND test_wifi_config_jobs)
add_executable(test_app_factory_service test_app_factory_service.c
    ../support/legacy/app_factory_service.c ../../components/app_core/app_factory_transaction.c
    ../../common/network_config.c)
target_include_directories(test_app_factory_service PRIVATE ../support/legacy factory_stubs router_stubs
    ../../components/app_core/private ../../components/app_wifi/include)
target_compile_options(test_app_factory_service PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_factory_service COMMAND test_app_factory_service)
add_executable(test_factory_stop test_factory_stop.c
    ../support/legacy/app_factory_service.c ../../components/app_core/app_factory_transaction.c
    ../../common/network_config.c)
target_include_directories(test_factory_stop PRIVATE ../support/legacy factory_stubs router_stubs
    ../../components/app_core/private ../../components/app_wifi/include)
target_compile_options(test_factory_stop PRIVATE -Wall -Wextra -Werror)
add_test(NAME factory_stop COMMAND test_factory_stop)
add_executable(test_core_network_stop test_core_network_stop.c
    ../../components/app_core/app_core_network.c ../../components/app_wifi/app_wifi.c ../../common/network_config.c ../../common/async_token.c)
target_include_directories(test_core_network_stop PRIVATE display_stubs router_stubs
    ../../components/app_core/include ../../components/app_core/private ../../components/app_wifi/include
    ../../components/app_wifi_messages/include)
target_compile_options(test_core_network_stop PRIVATE -Wall -Wextra -Werror)
add_test(NAME core_network_stop COMMAND test_core_network_stop)
add_executable(test_app_wifi test_app_wifi.c ../../components/app_wifi/app_wifi.c ../../common/network_config.c)
target_include_directories(test_app_wifi PRIVATE ../../components/app_wifi/include)
target_compile_options(test_app_wifi PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_wifi COMMAND test_app_wifi)
add_executable(test_wifi_saved_config test_wifi_saved_config.c
    ../../components/wifi_esp32/wifi_saved_config.c ../../common/network_config.c)
target_include_directories(test_wifi_saved_config PRIVATE wifi_store_stubs display_stubs
    ../../components/wifi_esp32/private ../../components/app_wifi/include)
target_compile_options(test_wifi_saved_config PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_saved_config COMMAND test_wifi_saved_config)
add_executable(test_app_wifi_messages test_app_wifi_messages.c
    ../../components/app_wifi_messages/app_wifi_messages.c
    ../../components/app_wifi/app_wifi.c ../../common/network_config.c)
target_include_directories(test_app_wifi_messages PRIVATE router_stubs display_stubs ../../common
    ../../components/app_wifi_messages/include ../../components/app_wifi/include ../../components/app_console/include)
target_compile_options(test_app_wifi_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_wifi_messages COMMAND test_app_wifi_messages)
add_executable(test_app_message_router test_app_message_router.c
    ../../components/app_console/app_message.c ../../components/app_console/app_message_router.c)
target_include_directories(test_app_message_router PRIVATE router_stubs display_stubs
    ../../components/app_console/include ../../components/app_console/private ../../common)
target_compile_options(test_app_message_router PRIVATE -Wall -Wextra -Werror)
add_test(NAME app_message_router COMMAND test_app_message_router)
add_executable(test_display_surface test_display_surface.c ../../components/display_surface/display_surface.c)
target_include_directories(test_display_surface PRIVATE display_stubs
    ../../components/display_surface/include ../../components/board_7b/include)
target_compile_options(test_display_surface PRIVATE -Wall -Wextra -Werror)
add_test(NAME display_surface COMMAND test_display_surface)
add_test(NAME display_surface_init_failure COMMAND test_display_surface init_failure)
add_executable(test_ui_model test_ui_model.c ../../components/app_ui/ui_model.c ../../components/app_ui/camera_settings.c)
target_sources(test_ui_model PRIVATE ../support/ui_vendor_model_legacy.c)
target_include_directories(test_ui_model PRIVATE display_stubs ../../components/app_ui/include
    ../../components/app_ui/private ../../common)
target_compile_options(test_ui_model PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_model COMMAND test_ui_model)
add_executable(test_ble_advertisement test_ble_advertisement.c ../../m5_atom_matrix/main/ble_advertisement.c)
target_include_directories(test_ble_advertisement PRIVATE ../../m5_atom_matrix/main)
target_compile_options(test_ble_advertisement PRIVATE -Wall -Wextra -Werror)
add_test(NAME ble_advertisement COMMAND test_ble_advertisement)
foreach(name ota_health maint_notice)
    add_executable(test_${name} test_${name}.c)
    target_include_directories(test_${name} PRIVATE ../support/legacy ../../components/app_core/private ../../common)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
add_executable(test_image_stride test_image_stride.c)
target_include_directories(test_image_stride PRIVATE ../../common)
target_compile_options(test_image_stride PRIVATE -Wall -Wextra -Werror)
add_test(NAME image_stride COMMAND test_image_stride)
add_executable(test_ota_header test_ota_header.c ../../components/app_maintenance/ota_header.c)
target_include_directories(test_ota_header PRIVATE ../../components/app_maintenance/private)
target_compile_options(test_ota_header PRIVATE -Wall -Wextra -Werror)
add_test(NAME ota_header COMMAND test_ota_header)
add_executable(test_restart_schedule test_restart_schedule.c)
target_include_directories(test_restart_schedule PRIVATE ../../components/app_core/private)
target_compile_options(test_restart_schedule PRIVATE -Wall -Wextra -Werror)
add_test(NAME restart_schedule COMMAND test_restart_schedule)
add_executable(test_maint_confirm test_maint_confirm.c)
target_include_directories(test_maint_confirm PRIVATE ../support/legacy)
target_compile_options(test_maint_confirm PRIVATE -Wall -Wextra -Werror)
add_test(NAME maint_confirm COMMAND test_maint_confirm)
add_executable(test_maint_auth test_maint_auth.c ../support/legacy/maint_auth.c)
target_include_directories(test_maint_auth PRIVATE ../support/legacy)
target_compile_options(test_maint_auth PRIVATE -Wall -Wextra -Werror)
add_test(NAME maint_auth COMMAND test_maint_auth)
add_executable(test_maint_json test_maint_json.c ../../components/app_maintenance/maint_json.c)
target_include_directories(test_maint_json PRIVATE ../../components/app_maintenance/private)
target_compile_options(test_maint_json PRIVATE -Wall -Wextra -Werror)
add_test(NAME maint_json COMMAND test_maint_json)
add_executable(test_maint_wifi test_maint_wifi.c ../../components/app_maintenance/maint_wifi.c ../../common/network_config.c)
target_include_directories(test_maint_wifi PRIVATE ../../components/app_maintenance/private)
target_compile_options(test_maint_wifi PRIVATE -Wall -Wextra -Werror)
add_test(NAME maint_wifi COMMAND test_maint_wifi)
add_executable(test_atom_fault test_atom_fault.c ../../common/atom_fault.c)
target_include_directories(test_atom_fault PRIVATE ../../common)
set_property(TARGET test_atom_fault PROPERTY C_STANDARD 11)
target_compile_options(test_atom_fault PRIVATE -Wall -Wextra -Werror)
add_test(NAME atom_fault COMMAND test_atom_fault)
add_executable(test_i2c_monitor test_i2c_monitor.c ../../common/i2c_monitor.c ../../common/i2c_monitor_format.c ../../common/atom_protocol.c)
target_include_directories(test_i2c_monitor PRIVATE ../../common)
target_compile_options(test_i2c_monitor PRIVATE -Wall -Wextra -Werror)
add_test(NAME i2c_monitor COMMAND test_i2c_monitor)
foreach(name pad_cmd pad_player)
    add_executable(test_${name} test_${name}.c ../../common/pad_cmd.c ../../common/pad_player.c)
    target_include_directories(test_${name} PRIVATE ../../common)
    set_property(TARGET test_${name} PROPERTY C_STANDARD 11)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME liveview_log_analysis COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_analyze_liveview.py)
add_test(NAME component_graph_contract COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_component_graph.py)
add_test(NAME module_symbol_contract COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_module_symbols.py)
add_executable(test_core_wifi_boot test_core_wifi_boot.c ../../components/app_core/app_core_wifi_boot.c ../../common/network_config.c)
target_include_directories(test_core_wifi_boot PRIVATE display_stubs ../../components/app_core/private ../../components/app_wifi/include ../../components/wifi_esp32/include ../../common)
target_compile_options(test_core_wifi_boot PRIVATE -Wall -Wextra -Werror)
foreach(case normal invalid create_failed read_failed get_failed init_failed start_failed)
    add_test(NAME core_wifi_boot_${case} COMMAND test_core_wifi_boot ${case})
endforeach()
add_test(NAME module_boundaries COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/check_module_boundaries.py)
add_test(NAME storage_owner_gate COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_storage_owner_gate.py)
add_test(NAME uart_script COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_uart_script.py)
add_test(NAME doc_links COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_doc_links.py)
add_executable(test_debug_line test_debug_line.c ../../common/debug_line.c)
target_include_directories(test_debug_line PRIVATE ../../common)
target_compile_options(test_debug_line PRIVATE -Wall -Wextra -Werror)
add_test(NAME debug_line COMMAND test_debug_line)
add_executable(test_sony_liveview test_sony_liveview.c ../../components/camera_backend_sony/sony_liveview.c)
target_include_directories(test_sony_liveview PRIVATE ../../components/camera_backend_sony/private)
add_executable(test_board_lcd test_board_lcd.c ../../components/board_7b/board_lcd.c)
target_include_directories(test_board_lcd PRIVATE display_stubs ../../components/board_7b/private)
target_compile_definitions(test_board_lcd PRIVATE CONFIG_APP_DEBUG_FAULT_INJECTION=1)
add_executable(test_liveview_pipeline test_liveview_pipeline.c ../support/legacy/liveview_pipeline.c ../../components/camera_backend_sony/sony_liveview.c)
target_include_directories(test_liveview_pipeline PRIVATE display_stubs ../support/legacy ../../components/app_ui/include ../../components/camera_backend_sony/private)
foreach(name sony_liveview board_lcd liveview_pipeline)
    set_property(TARGET test_${name} PROPERTY C_STANDARD 11)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
add_executable(test_factory_reset test_factory_reset.c ../../components/app_core/app_factory_transaction.c ../../common/network_config.c)
target_include_directories(test_factory_reset PRIVATE ../../components/app_core/private display_stubs)
target_compile_options(test_factory_reset PRIVATE -Wall -Wextra -Werror)
add_test(NAME factory_reset COMMAND test_factory_reset)
add_executable(test_wifi_menu test_wifi_menu.c ../support/legacy/wifi_menu.c ../../common/network_config.c)
target_include_directories(test_wifi_menu PRIVATE ../support/legacy)
target_compile_options(test_wifi_menu PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_menu COMMAND test_wifi_menu)
add_executable(test_wifi_config test_wifi_config.c ../../common/network_config.c)
add_executable(test_wifi_apply test_wifi_apply.c ../../components/app_wifi/wifi_apply.c ../../common/network_config.c)
add_executable(test_debug_args test_debug_args.c ../../common/debug_args.c)
foreach(target wifi_config wifi_apply debug_args)
    target_include_directories(test_${target} PRIVATE ../../main ../../common)
    target_compile_options(test_${target} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${target} COMMAND test_${target})
endforeach()
add_executable(test_atom_slave_tx test_atom_slave_tx.c ../../m5_atom_matrix/main/atom_slave_tx.c)
target_include_directories(test_atom_slave_tx PRIVATE tx_stubs stubs ../../m5_atom_matrix/main)
target_compile_options(test_atom_slave_tx PRIVATE -Wall -Wextra -Werror)
add_test(NAME atom_slave_tx COMMAND test_atom_slave_tx)
add_executable(test_atom_protocol test_atom_protocol.c ../../common/atom_protocol.c ../../common/atom_client.c)
target_include_directories(test_atom_protocol PRIVATE ../../common)
target_compile_options(test_atom_protocol PRIVATE -Wall -Wextra -Werror)
add_test(NAME atom_protocol COMMAND test_atom_protocol)
foreach(name ds4_events ds4_report matrix_model)
    add_executable(test_${name} ../../m5_atom_matrix/tests/test_${name}.c ../../m5_atom_matrix/main/${name}.c)
    target_include_directories(test_${name} PRIVATE ../../common ../../m5_atom_matrix/main)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
foreach(name gamepad_input)
    add_executable(test_${name} test_${name}.c ../../components/app_input/${name}.c)
    target_include_directories(test_${name} PRIVATE ../../main ../../common ../../components/app_input/private)
    set_property(TARGET test_${name} PROPERTY C_STANDARD 11)
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()
add_executable(test_setting_control test_setting_control.c ../../components/app_camera/setting_control.c)
target_include_directories(test_setting_control PRIVATE ../../main ../../components/camera_backend_sony/private)
target_compile_options(test_setting_control PRIVATE -Wall -Wextra -Werror)
add_test(NAME setting_control COMMAND test_setting_control)
add_executable(test_camera_menu test_camera_menu.c ../../components/app_camera/camera_menu.c ../support/legacy/camera_menu_sony_legacy.c ../../components/app_camera/setting_control.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_camera_menu PRIVATE ../support/legacy ../../components/camera_backend_sony/private ../../components/ptpip/include)
target_compile_options(test_camera_menu PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_menu COMMAND test_camera_menu ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sony-props-193841.bin)
add_executable(test_camera_parsers test_camera_parsers.c
    ../../components/ptpip/ptp_dataset.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_camera_parsers PRIVATE
    ../../components/ptpip/include ../../components/camera_backend_sony/private)
set_property(TARGET test_camera_parsers PROPERTY C_STANDARD 11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(test_camera_parsers PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME camera_parsers COMMAND test_camera_parsers)

add_executable(test_focus_input test_focus_input.c ../support/legacy/focus_input.c)
target_include_directories(test_focus_input PRIVATE ../support/legacy)
add_test(NAME focus_input COMMAND test_focus_input)
add_executable(test_focus_caps test_focus_caps.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_focus_caps PRIVATE ../../components/ptpip/include ../../components/camera_backend_sony/private)
add_test(NAME focus_caps COMMAND test_focus_caps)
add_executable(test_sony_descriptors test_sony_descriptors.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_sony_descriptors PRIVATE ../../components/ptpip/include ../../components/camera_backend_sony/private)
add_test(NAME sony_descriptors COMMAND test_sony_descriptors)
add_executable(test_property_fixtures test_property_fixtures.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_property_fixtures PRIVATE ../../components/ptpip/include ../../components/camera_backend_sony/private)
add_test(NAME props_193841 COMMAND test_property_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sony-props-193841.bin 1 1)
add_test(NAME props_195145 COMMAND test_property_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sony-props-195145.bin 32772 1)
add_test(NAME props_200352 COMMAND test_property_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sony-props-200352.bin 1 0)
add_test(NAME props_200950 COMMAND test_property_fixtures ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/sony-props-200950.bin 1 1)
add_executable(test_sony_write test_sony_write.c ../support/legacy/sony_ext.c ../../components/camera_backend_sony/sony_control_encoder.c ../support/legacy/ptp_send_data.c ../../components/ptpip/ptp_wire.c)
target_include_directories(test_sony_write PRIVATE stubs ../../components/ptpip/include ../../components/camera_backend_sony/private)
target_sources(test_sony_write PRIVATE ../support/legacy/sony_exposure_encoder.c)
add_test(NAME sony_write COMMAND test_sony_write)
foreach(target test_focus_input test_focus_caps test_sony_write test_sony_descriptors test_property_fixtures)
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endforeach()
add_executable(test_input_reports test_input_reports.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c)
target_include_directories(test_input_reports PRIVATE ../../components/app_input/private ../../common)
set_property(TARGET test_input_reports PROPERTY C_STANDARD 11)
target_compile_options(test_input_reports PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_reports COMMAND test_input_reports)
add_executable(test_input_provider test_input_provider.c ../../components/app_input/input_provider.c)
target_include_directories(test_input_provider PRIVATE router_stubs display_stubs ../../components/app_input/include ../../components/app_input/private)
set_property(TARGET test_input_provider PROPERTY C_STANDARD 11)
target_compile_options(test_input_provider PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_provider COMMAND test_input_provider)
add_executable(test_input_owner test_input_owner.c ../../components/app_input/input_owner.c ../../components/app_input/input_provider.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c)
target_include_directories(test_input_owner PRIVATE router_stubs display_stubs ../../components/app_input/include ../../components/app_input/private ../../common)
set_property(TARGET test_input_owner PROPERTY C_STANDARD 11)
target_compile_options(test_input_owner PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_owner COMMAND test_input_owner)
add_executable(test_input_equivalence test_input_equivalence.c ../../components/app_input/input_owner.c
    ../../components/app_input/input_provider.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c)
target_include_directories(test_input_equivalence PRIVATE router_stubs display_stubs
    ../../components/app_input/include ../../components/app_input/private ../../common)
set_property(TARGET test_input_equivalence PROPERTY C_STANDARD 11)
target_compile_options(test_input_equivalence PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_equivalence COMMAND test_input_equivalence)
add_executable(test_ui_input_messages test_ui_input_messages.c ../../components/app_ui/ui_input_messages.c)
target_include_directories(test_ui_input_messages PRIVATE display_stubs ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../common)
target_compile_options(test_ui_input_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_input_messages COMMAND test_ui_input_messages)
add_executable(test_ui_menu_messages test_ui_menu_messages.c ../../components/app_ui/ui_menu_messages.c ../../components/app_ui/ui_model.c ../../components/app_ui/camera_settings.c ../../common/ui_overlay.c)
target_include_directories(test_ui_menu_messages PRIVATE display_stubs ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../common)
target_compile_options(test_ui_menu_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_menu_messages COMMAND test_ui_menu_messages)
add_executable(test_ui_preferences test_ui_preferences.c ../support/legacy/ui_preferences.c ../../common/async_token.c ../../common/ui_overlay.c)
target_include_directories(test_ui_preferences PRIVATE ../support/legacy router_stubs display_stubs wifi_store_stubs ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../common)
target_compile_options(test_ui_preferences PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_preferences COMMAND test_ui_preferences)
target_sources(test_ui_preferences PRIVATE ../../components/app_wifi/app_wifi.c ../../common/network_config.c)
target_include_directories(test_ui_preferences PRIVATE ../../components/app_wifi/include)
foreach(target test_app_wifi test_app_wifi_channels test_wifi_config_jobs test_app_wifi_messages test_wifi_channel_messages)
    target_sources(${target} PRIVATE ../../common/async_token.c)
    target_include_directories(${target} PRIVATE ../../common)
endforeach()

add_executable(test_camera_settings test_camera_settings.c
    ../../components/app_ui/camera_settings.c ../../components/camera_backend_sony/sony_props.c)
target_include_directories(test_camera_settings PRIVATE ../../components/app_ui/private
    ../../components/camera_backend_sony/private ../../components/ptpip/include)
target_compile_options(test_camera_settings PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_settings COMMAND test_camera_settings)

add_executable(test_camera_link test_camera_link.c ../../components/app_camera/camera_link.c)
target_include_directories(test_camera_link PRIVATE ../../main)
add_test(NAME camera_link COMMAND test_camera_link)
add_executable(test_ptp_session test_ptp_session.c ../support/legacy/ptp_session.c ../../components/ptpip/ptp_wire.c)
target_include_directories(test_ptp_session PRIVATE network_stubs stubs ../../components/ptpip/include)
add_test(NAME ptp_session COMMAND test_ptp_session)
add_executable(test_ptpip_transport test_ptpip_transport.c ../support/legacy/ptpip_transport.c)
target_include_directories(test_ptpip_transport PRIVATE network_stubs stubs ../../components/ptpip/include)
target_compile_definitions(test_ptpip_transport PRIVATE fcntl=net_fcntl)
add_test(NAME ptpip_transport COMMAND test_ptpip_transport)
foreach(target test_camera_link test_ptp_session test_ptpip_transport)
    set_property(TARGET ${target} PROPERTY C_STANDARD 11)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
endforeach()

add_executable(test_camera_identity test_camera_identity.c ../../components/common_runtime/camera_identity_store.c)
target_include_directories(test_camera_identity PRIVATE identity_stubs stubs ../../common)
target_compile_options(test_camera_identity PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_identity COMMAND test_camera_identity)

add_executable(test_debug_hex test_debug_hex.c ../../common/debug_hex.c)
target_include_directories(test_debug_hex PRIVATE ../../common)
target_compile_options(test_debug_hex PRIVATE -Wall -Wextra -Werror)
add_test(NAME debug_hex COMMAND test_debug_hex)

add_executable(test_matrix_debug test_matrix_debug.c ../../m5_atom_matrix/main/matrix_debug.c ../../m5_atom_matrix/main/matrix_model.c)
target_include_directories(test_matrix_debug PRIVATE ../../m5_atom_matrix/main)
target_compile_options(test_matrix_debug PRIVATE -Wall -Wextra -Werror)
add_test(NAME matrix_debug COMMAND test_matrix_debug)

add_executable(test_atom_sim test_atom_sim.c ../../common/atom_sim.c ../../common/atom_protocol.c ../../common/atom_client.c ../../m5_atom_matrix/main/ds4_events.c)
target_include_directories(test_atom_sim PRIVATE ../../common)
target_compile_options(test_atom_sim PRIVATE -Wall -Wextra -Werror)
add_test(NAME atom_sim COMMAND test_atom_sim)

add_executable(test_ui_overlay test_ui_overlay.c ../../common/ui_overlay.c)
target_include_directories(test_ui_overlay PRIVATE ../../common)
target_compile_options(test_ui_overlay PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_overlay COMMAND test_ui_overlay)

add_executable(test_ultimate2_report test_ultimate2_report.c ../../m5_atom_matrix/main/ultimate2_report.c)
target_include_directories(test_ultimate2_report PRIVATE ../../m5_atom_matrix/main ../../common)
target_compile_options(test_ultimate2_report PRIVATE -Wall -Wextra -Werror)
add_test(NAME ultimate2_report COMMAND test_ultimate2_report)

add_executable(test_pad_publish test_pad_publish.c ../../m5_atom_matrix/main/pad_publish.c ../../m5_atom_matrix/main/ds4_events.c)
target_include_directories(test_pad_publish PRIVATE ../../m5_atom_matrix/main ../../common)
target_compile_options(test_pad_publish PRIVATE -Wall -Wextra -Werror)
add_test(NAME pad_publish COMMAND test_pad_publish)

foreach(target test_maint_wifi test_factory_reset test_wifi_menu test_wifi_config test_wifi_apply)
    target_include_directories(${target} PRIVATE ../../components/app_wifi/include)
endforeach()

add_executable(test_camera_backend test_camera_backend.c ../../components/camera_backend/camera_backend.c)
target_include_directories(test_camera_backend PRIVATE ../../components/camera_backend/include)
set_property(TARGET test_camera_backend PROPERTY C_STANDARD 11)
target_compile_options(test_camera_backend PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_backend COMMAND test_camera_backend)

add_executable(test_sony_backend test_sony_backend.c
    ../../components/camera_backend/camera_backend.c
    ../../components/camera_backend_sony/camera_backend_sony.c
    ../../components/camera_backend_sony/sony_props.c ../../components/camera_backend_sony/sony_liveview.c
    ../../components/camera_backend_sony/sony_control_encoder.c
    ../../components/ptpip/ptpip_protocol.c ../../components/ptpip/ptpip_client.c
    ../../components/ptpip/ptp_wire.c ../../components/ptpip/ptp_dataset.c
    ../../components/app_console/app_message.c)
target_include_directories(test_sony_backend PRIVATE router_stubs display_stubs ../../common
    ../../components/camera_backend/include ../../components/camera_backend_sony/private
    ../../components/camera_backend_sony/private ../../components/ptpip/include
    ../../components/app_console/include ../../components/app_console/private)
set_property(TARGET test_sony_backend PROPERTY C_STANDARD 11)
target_compile_options(test_sony_backend PRIVATE -Wall -Wextra -Werror)
target_sources(test_sony_backend PRIVATE ../support/legacy/sony_exposure_encoder.c)
add_test(NAME sony_backend COMMAND test_sony_backend)

foreach(target test_setting_control test_camera_menu test_camera_parsers test_focus_caps
    test_sony_descriptors test_property_fixtures test_camera_settings)
    target_include_directories(${target} PRIVATE ../../common)
endforeach()

add_executable(test_camera_properties test_camera_properties.c ../../components/app_camera/camera_properties.c
    ../../components/app_camera/camera_menu.c ../../components/app_camera/setting_control.c)
target_include_directories(test_camera_properties PRIVATE ../../main ../../common ../../components/camera_backend/include)
target_compile_options(test_camera_properties PRIVATE -Wall -Wextra -Werror)
set_property(TARGET test_camera_properties PROPERTY C_STANDARD 11)
add_test(NAME camera_properties COMMAND test_camera_properties)

foreach(target test_setting_control test_camera_menu test_camera_properties)
    target_include_directories(${target} PRIVATE ../../components/app_camera/private)
endforeach()

add_executable(test_camera_session test_camera_session.c ../../components/app_camera/camera_session.c
    ../../components/camera_backend/camera_backend.c)
target_include_directories(test_camera_session PRIVATE ../../components/app_camera/private
    ../../components/camera_backend/include)
set_property(TARGET test_camera_session PROPERTY C_STANDARD 11)
target_compile_options(test_camera_session PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_session COMMAND test_camera_session)

add_executable(test_camera_discovery test_camera_discovery.c ../../components/app_camera/camera_discovery.c
    ../../components/app_camera/camera_session.c ../../components/app_camera/camera_link.c
    ../../components/camera_backend/camera_backend.c)
target_include_directories(test_camera_discovery PRIVATE router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/camera_backend/include ../../components/app_console/include)
set_property(TARGET test_camera_discovery PROPERTY C_STANDARD 11)
target_compile_options(test_camera_discovery PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_discovery COMMAND test_camera_discovery)
add_executable(test_camera_actions test_camera_actions.c ../../components/app_camera/camera_actions.c)
target_include_directories(test_camera_actions PRIVATE ../../components/app_camera/private ../../common ../../components/camera_backend/include)
target_compile_options(test_camera_actions PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_actions COMMAND test_camera_actions)

add_executable(test_camera_controls test_camera_controls.c
    ../../components/app_camera/camera_controls.c ../../components/app_camera/camera_actions.c
    ../../components/app_camera/camera_properties.c ../../components/app_camera/camera_menu.c
    ../../components/app_camera/setting_control.c)
target_include_directories(test_camera_controls PRIVATE ../../components/app_camera/private
    ../../components/camera_backend/include ../../components/app_console/include ../../common display_stubs)
target_compile_options(test_camera_controls PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_controls COMMAND test_camera_controls)

add_executable(test_ui_camera_messages test_ui_camera_messages.c ../../components/app_ui/ui_camera_messages.c
    ../../components/app_ui/ui_model.c ../../components/app_ui/camera_settings.c ../../components/app_console/app_message.c)
target_include_directories(test_ui_camera_messages PRIVATE router_stubs display_stubs ../../common
    ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_ui_camera_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_camera_messages COMMAND test_ui_camera_messages)

add_executable(test_camera_outputs test_camera_outputs.c ../../components/app_camera/camera_outputs.c
    ../../components/app_camera/camera_menu.c ../../components/app_camera/setting_control.c ../../components/app_console/app_message.c)
set_source_files_properties(../../components/app_camera/camera_outputs.c PROPERTIES COMPILE_DEFINITIONS "calloc=test_output_calloc;free=test_output_free")
target_include_directories(test_camera_outputs PRIVATE router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/camera_backend/include ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_camera_outputs PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_outputs COMMAND test_camera_outputs)

target_include_directories(test_camera_link PRIVATE ../../components/app_camera/private)

add_executable(test_camera_frames test_camera_frames.c ../../components/app_camera/camera_frames.c
    ../../components/app_console/app_message.c)
target_include_directories(test_camera_frames PRIVATE router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_camera_frames PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_frames COMMAND test_camera_frames)

add_executable(test_ui_frames test_ui_frames.c ../../components/app_ui/ui_frames.c
    ../../components/app_console/app_message.c)
target_include_directories(test_ui_frames PRIVATE router_stubs display_stubs ../../common
    ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_ui_frames PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_frames COMMAND test_ui_frames)

add_executable(test_frame_bus_integration test_frame_bus_integration.c
    ../../components/app_camera/camera_frames.c ../../components/app_ui/ui_frames.c
    ../../components/app_console/app_message.c ../../components/app_console/app_message_router.c)
target_include_directories(test_frame_bus_integration PRIVATE router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/app_ui/include ../../components/app_ui/private
    ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_frame_bus_integration PRIVATE -Wall -Wextra -Werror)
add_test(NAME frame_bus_integration COMMAND test_frame_bus_integration)

add_executable(test_camera_settings_execute test_camera_settings_execute.c
    ../../components/app_camera/camera_settings_execute.c ../../components/app_camera/camera_properties.c
    ../../components/app_camera/camera_menu.c ../../components/app_camera/setting_control.c)
target_include_directories(test_camera_settings_execute PRIVATE ../../components/app_camera/private
    ../../components/camera_backend/include ../../common)
target_compile_options(test_camera_settings_execute PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_settings_execute COMMAND test_camera_settings_execute)

add_executable(test_camera_identity_work test_camera_identity_work.c ../../components/app_camera/camera_identity_work.c)
target_include_directories(test_camera_identity_work PRIVATE router_stubs ../../common ../../components/app_camera/private)
target_compile_options(test_camera_identity_work PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_identity_work COMMAND test_camera_identity_work)

add_executable(test_camera_stream test_camera_stream.c ../../components/app_camera/camera_stream.c
    ../../components/app_camera/camera_controls.c ../../components/app_camera/camera_actions.c
    ../../components/app_camera/camera_properties.c ../../components/app_camera/camera_menu.c
    ../../components/app_camera/setting_control.c ../../components/app_camera/camera_settings_execute.c
    ../../components/app_camera/camera_frames.c ../../components/app_camera/camera_outputs.c ../../components/app_console/app_message.c)
target_include_directories(test_camera_stream PRIVATE router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/camera_backend/include ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_camera_stream PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_stream COMMAND test_camera_stream)

add_executable(test_camera_endpoint test_camera_endpoint.c)
target_include_directories(test_camera_endpoint PRIVATE router_stubs display_stubs ../../common ../../components/app_console/include ../../components/app_camera/private)
target_compile_options(test_camera_endpoint PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_endpoint COMMAND test_camera_endpoint)
add_executable(test_camera_endpoint_debug test_camera_endpoint.c)
target_include_directories(test_camera_endpoint_debug PRIVATE router_stubs display_stubs ../../common ../../components/app_console/include ../../components/app_camera/private)
target_compile_definitions(test_camera_endpoint_debug PRIVATE CONFIG_REMOTE_DBG_SIM=1)
target_compile_options(test_camera_endpoint_debug PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_endpoint_debug COMMAND test_camera_endpoint_debug)

add_executable(test_camera_producer test_camera_producer.c ../../components/app_camera/camera_session.c
    ../../components/app_camera/camera_discovery.c ../../components/app_camera/camera_link.c
    ../../components/camera_backend/camera_backend.c ../../components/app_camera/camera_stream.c
    ../../components/app_camera/camera_controls.c ../../components/app_camera/camera_actions.c
    ../../components/app_camera/camera_properties.c ../../components/app_camera/camera_menu.c ../../components/app_camera/setting_control.c
    ../../components/app_camera/camera_settings_execute.c ../../components/app_camera/camera_frames.c ../../components/app_camera/camera_outputs.c
    ../../components/app_console/app_message.c)
target_include_directories(test_camera_producer PRIVATE camera_runtime_stubs router_stubs display_stubs ../../common
    ../../components/app_camera/private ../../components/app_camera/include ../../components/camera_backend/include
    ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_camera_producer PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_producer COMMAND test_camera_producer)

add_executable(test_core_camera_policy test_core_camera_policy.c ../../components/app_core/app_core_camera.c ../../components/app_core/app_core_mode.c)
target_include_directories(test_core_camera_policy PRIVATE router_stubs display_stubs ../../common
    ../../components/app_core/include ../../components/app_core/private ../../components/app_camera/include ../../components/app_console/include)
target_compile_options(test_core_camera_policy PRIVATE -Wall -Wextra -Werror)
add_test(NAME core_camera_policy COMMAND test_core_camera_policy)

foreach(target test_sony_write test_ptp_session test_ptpip_transport)
    target_include_directories(${target} PRIVATE ../support/legacy ../../components/ptpip/private)
endforeach()

add_executable(test_ui_wifi_menu test_ui_wifi_menu.c ../support/legacy/wifi_menu_ui_noreset.c ../support/legacy/wifi_menu_noreset.c ../support/legacy/ui_menu_wifi_messages.c ../support/legacy/ui_model_wifi.c ../../components/app_ui/camera_settings.c ../../common/network_config.c ../../common/ui_overlay.c)
target_include_directories(test_ui_wifi_menu PRIVATE ../support/legacy router_stubs display_stubs identity_stubs ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include ../../common)
target_compile_options(test_ui_wifi_menu PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_wifi_menu COMMAND test_ui_wifi_menu)

target_include_directories(test_core_factory_messages PRIVATE ../../components/app_input/include)

add_executable(test_input_service test_input_service.c ../../components/app_input/input_owner.c ../../components/app_input/input_provider.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c)
target_include_directories(test_input_service PRIVATE router_stubs display_stubs ../../components/app_input/include ../../components/app_input/private ../../components/app_console/include ../../common)
target_compile_options(test_input_service PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_service COMMAND test_input_service)

add_executable(test_input_sim_provider test_input_sim_provider.c ../../components/app_input/input_provider.c ../../components/app_input/input_owner.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c ../../common/atom_sim.c ../../common/atom_client.c ../../common/atom_protocol.c ../../common/pad_player.c ../../m5_atom_matrix/main/ds4_events.c ../../components/app_console/app_message.c)
target_include_directories(test_input_sim_provider PRIVATE router_stubs display_stubs ../../common ../../components/app_input_sim/include ../../components/app_input/include ../../components/app_input/private ../../components/app_console/include ../../components/app_console/private)
target_compile_options(test_input_sim_provider PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_sim_provider COMMAND test_input_sim_provider)
add_executable(test_input_atom_provider test_input_atom_provider.c ../../components/app_input/input_provider.c ../../components/app_input/input_owner.c ../../components/app_input/input_reports.c ../../components/app_input/gamepad_input.c ../../common/atom_sim.c ../../common/atom_client.c ../../common/atom_protocol.c ../../common/pad_player.c ../../m5_atom_matrix/main/ds4_events.c ../../common/i2c_monitor.c ../../common/i2c_monitor_format.c)
target_include_directories(test_input_atom_provider PRIVATE input_stubs router_stubs display_stubs ../../common ../../components/app_input_atom/include ../../components/app_input/include ../../components/app_input/private ../../components/app_console/include)
target_compile_options(test_input_atom_provider PRIVATE -Wall -Wextra -Werror)
add_test(NAME input_atom_provider COMMAND test_input_atom_provider)

foreach(kind debug release)
    add_executable(test_core_input_${kind} test_core_input_policy.c ../../components/app_core/app_core_input.c)
    target_include_directories(test_core_input_${kind} PRIVATE display_stubs ../../components/app_core/private ../../components/app_core/include ../../components/app_input_atom/include ../../components/app_input_sim/include)
    target_compile_options(test_core_input_${kind} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME core_input_${kind} COMMAND test_core_input_${kind})
endforeach()
target_compile_definitions(test_core_input_debug PRIVATE CONFIG_REMOTE_DBG_SIM=1)
target_compile_definitions(test_core_input_release PRIVATE CONFIG_REMOTE_DBG_SIM=0)

target_compile_definitions(test_input_service PRIVATE CONFIG_REMOTE_DBG_SIM=1)

get_target_property(input_service_sources test_input_service SOURCES)
get_target_property(input_service_includes test_input_service INCLUDE_DIRECTORIES)
add_executable(test_input_service_release ${input_service_sources})
target_include_directories(test_input_service_release PRIVATE ${input_service_includes})
target_compile_options(test_input_service_release PRIVATE -Wall -Wextra -Werror)
# Leave disabled Kconfig undefined, matching ESP-IDF sdkconfig.h.
add_test(NAME input_service_release COMMAND test_input_service_release)

add_executable(test_uart_wifi_messages test_uart_wifi_messages.c ../../common/network_config.c)
target_include_directories(test_uart_wifi_messages PRIVATE identity_stubs display_stubs ../../common
    ../../components/app_console/include)
target_compile_options(test_uart_wifi_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_wifi_messages COMMAND test_uart_wifi_messages)

add_executable(test_uart_preferences test_uart_preferences.c)
target_include_directories(test_uart_preferences PRIVATE display_stubs ../../common
    ../../components/app_console/include)
target_compile_options(test_uart_preferences PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_preferences COMMAND test_uart_preferences)

add_executable(test_uart_camera_commands test_uart_camera_commands.c)
target_include_directories(test_uart_camera_commands PRIVATE display_stubs ../../common ../../components/app_console/include)
target_compile_options(test_uart_camera_commands PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_camera_commands COMMAND test_uart_camera_commands)
target_compile_definitions(test_uart_camera_commands PRIVATE CONFIG_REMOTE_DBG_SIM=1)
add_executable(test_uart_camera_commands_release test_uart_camera_commands.c)
target_include_directories(test_uart_camera_commands_release PRIVATE display_stubs ../../common ../../components/app_console/include)
target_compile_options(test_uart_camera_commands_release PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_camera_commands_release COMMAND test_uart_camera_commands_release)

add_executable(test_uart_status test_uart_status.c)
target_include_directories(test_uart_status PRIVATE uart_stubs display_stubs ../../common ../../components/app_console/include)
target_compile_options(test_uart_status PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_status COMMAND test_uart_status)

add_executable(test_ui_bench test_ui_bench.c)
target_include_directories(test_ui_bench PRIVATE bench_stubs router_stubs display_stubs ../../common
    ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include)
target_compile_options(test_ui_bench PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_bench COMMAND test_ui_bench)

add_executable(test_uart_bench test_uart_bench.c)
target_include_directories(test_uart_bench PRIVATE display_stubs ../../common ../../components/app_console/include)
target_compile_options(test_uart_bench PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_bench COMMAND test_uart_bench)

add_executable(test_uart_input_commands test_uart_input_commands.c ../../components/app_console/app_message.c ../../common/pad_cmd.c ../../common/i2c_monitor_format.c)
target_include_directories(test_uart_input_commands PRIVATE display_stubs ../../common ../../components/app_console/include ../../components/app_console/private)
target_compile_definitions(test_uart_input_commands PRIVATE CONFIG_REMOTE_DBG_SIM=1)
target_compile_options(test_uart_input_commands PRIVATE -Wall -Wextra -Werror)
add_test(NAME uart_input_commands COMMAND test_uart_input_commands)

get_target_property(camera_producer_sources test_camera_producer SOURCES)
get_target_property(camera_producer_includes test_camera_producer INCLUDE_DIRECTORIES)
add_executable(test_camera_producer_debug ${camera_producer_sources})
target_include_directories(test_camera_producer_debug PRIVATE ${camera_producer_includes})
target_compile_definitions(test_camera_producer_debug PRIVATE CONFIG_REMOTE_DBG_SIM=1)
target_compile_options(test_camera_producer_debug PRIVATE -Wall -Wextra -Werror)
add_test(NAME camera_producer_debug COMMAND test_camera_producer_debug)
add_test(NAME camera_producer_partial COMMAND test_camera_producer partial)
add_test(NAME camera_producer_debug_partial COMMAND test_camera_producer_debug partial)

foreach(target test_uart_bench test_uart_preferences test_uart_camera_commands test_uart_camera_commands_release test_uart_status test_uart_wifi_messages)
    target_include_directories(${target} PRIVATE ../../components/app_console/private)
endforeach()

add_executable(test_console_lifecycle test_console_lifecycle.c ../../common/debug_args.c ../../common/debug_line.c ../../common/async_token.c)
target_include_directories(test_console_lifecycle PRIVATE console_stubs router_stubs display_stubs ../../common)
target_compile_options(test_console_lifecycle PRIVATE -Wall -Wextra -Werror)
add_test(NAME console_lifecycle COMMAND test_console_lifecycle)

foreach(mode debug release)
    add_executable(test_uart_gateway_${mode} test_uart_gateway.c)
    target_include_directories(test_uart_gateway_${mode} PRIVATE console_stubs display_stubs ../../common ../../components/app_console/include ../../components/app_console/private)
    target_compile_options(test_uart_gateway_${mode} PRIVATE -Wall -Wextra -Werror)
    if(mode STREQUAL "debug")
        target_compile_definitions(test_uart_gateway_${mode} PRIVATE CONFIG_REMOTE_DBG_SIM=1)
    endif()
    add_test(NAME uart_gateway_${mode} COMMAND test_uart_gateway_${mode})
endforeach()

add_executable(test_ui_endpoint_stop test_ui_endpoint_stop.c ../../components/app_ui/ui_frames.c)
target_include_directories(test_ui_endpoint_stop PRIVATE bench_stubs router_stubs display_stubs ../../common ../../components/app_ui/include ../../components/app_ui/private ../../components/app_console/include)
target_compile_options(test_ui_endpoint_stop PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_endpoint_stop COMMAND test_ui_endpoint_stop)

add_executable(test_ui_render_stop test_ui_render_stop.c ../../components/app_ui/ui_render_lifecycle.c ../../components/app_ui/ui_model.c ../../components/app_ui/camera_settings.c ../../common/ui_overlay.c)
target_include_directories(test_ui_render_stop PRIVATE render_stubs bench_stubs router_stubs display_stubs ../../common ../../components/app_ui/include ../../components/app_ui/private ../../components/display_surface/include)
target_compile_options(test_ui_render_stop PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_render_stop COMMAND test_ui_render_stop)

# Actual isolated OTA translation unit: no Core/Camera/Input/UI/Console link.
add_executable(test_maintenance_ota test_maintenance_ota.c ../../components/app_maintenance/maintenance_ota.c ../../components/app_maintenance/ota_header.c)
target_include_directories(test_maintenance_ota PRIVATE maintenance_stubs core_start_stubs display_stubs ../../components/app_maintenance/include ../../components/app_maintenance/private)
target_compile_options(test_maintenance_ota PRIVATE -Wall -Wextra -Werror)
foreach(case success lost_reply allocation begin prefix_write body_write end set_boot interrupted busy reserve admission header shutdown)
    add_test(NAME maintenance_ota_${case} COMMAND test_maintenance_ota ${case})
endforeach()
add_executable(test_core_ota_health test_core_ota_health.c ../../components/app_core/app_core_ota_health.c)
target_include_directories(test_core_ota_health PRIVATE maintenance_stubs core_start_stubs display_stubs ../../components/app_core/private ../../components/app_core/include ../../components/app_wifi/include ../../components/app_ui/include ../../components/app_maintenance/include)
target_compile_options(test_core_ota_health PRIVATE -Wall -Wextra -Werror)
foreach(case confirm heap ap_offline ap_error ap_no_address confirm_fail display not_pending close_fail)
    add_test(NAME core_ota_health_${case} COMMAND test_core_ota_health ${case})
endforeach()
target_include_directories(test_ota_health PRIVATE ../../components/app_core/private)

target_include_directories(test_core_ota_health PRIVATE ../../components/app_console/include)

# Use the platform cJSON package, or the unchanged SDK source supplied explicitly.
# Firmware and this isolated Web contract test use the same parser API.
set(MODULE_CJSON_SOURCE_DIR "" CACHE PATH "Directory containing SDK cJSON.c and cJSON.h for host Web tests")
if(EXISTS "${MODULE_CJSON_SOURCE_DIR}/cJSON.c")
    add_library(module_test_json STATIC "${MODULE_CJSON_SOURCE_DIR}/cJSON.c")
    set(module_json_include "${MODULE_CJSON_SOURCE_DIR}")
else()
    find_path(module_json_include cJSON.h PATH_SUFFIXES cjson)
    find_library(module_json_library cjson)
    if(NOT module_json_include OR NOT module_json_library)
        message(FATAL_ERROR "Host Web tests need libcjson-dev or MODULE_CJSON_SOURCE_DIR pointing to SDK cJSON")
    endif()
    add_library(module_test_json INTERFACE)
    target_link_libraries(module_test_json INTERFACE "${module_json_library}")
endif()
add_executable(test_maintenance_web test_maintenance_web.c ../../components/app_maintenance/maintenance_trigger.c ../../components/app_maintenance/maintenance_web.c ../../components/app_maintenance/maint_json.c ../../components/app_maintenance/maint_wifi.c ../../common/network_config.c)
target_include_directories(test_maintenance_web PRIVATE "${module_json_include}" web_stubs maintenance_stubs core_start_stubs display_stubs ../../components/app_maintenance/include ../../components/app_maintenance/private ../../components/app_wifi/include)
target_link_libraries(test_maintenance_web PRIVATE module_test_json)
target_compile_options(test_maintenance_web PRIVATE -Wall -Wextra -Werror)
foreach(case normal settings save_failed lost_reply unavailable duplicate wifi_failed_ack wifi_failed_commit start_failed register_failed stop_failed version receive_stop receive_stop_last close_failed client_list_failed)
    add_test(NAME maintenance_web_${case} COMMAND test_maintenance_web ${case})
endforeach()
foreach(case trigger_known trigger_factory trigger_unknown trigger_method trigger_denied trigger_closed trigger_closed_during_claim trigger_error_register_failed trigger_start_failed trigger_stop_failed trigger_missing_activation)
    add_test(NAME maintenance_${case} COMMAND test_maintenance_web ${case})
endforeach()
foreach(case all wifi failed timeout lost_reply missing unconfirmed duplicate unknown extra busy unavailable)
    add_test(NAME maintenance_factory_${case} COMMAND test_maintenance_web factory_${case})
endforeach()
add_executable(test_core_factory_reset test_core_factory_reset.c ../../components/app_core/app_core_factory_reset.c ../../components/app_core/app_factory_transaction.c ../../common/network_config.c)
target_include_directories(test_core_factory_reset PRIVATE core_start_stubs display_stubs ../../common ../../components/app_core/include ../../components/app_core/private ../../components/app_wifi/include)
target_compile_options(test_core_factory_reset PRIVATE -Wall -Wextra -Werror)
foreach(case all wifi freeze_timeout freeze_failed read_failed write_failed identity_failed preferences_failed rollback_failed)
    add_test(NAME core_factory_reset_${case} COMMAND test_core_factory_reset ${case})
endforeach()
add_executable(test_core_settings test_core_settings.c ../../components/app_core/app_core_settings.c ../../components/common_runtime/preferences_store.c)
target_include_directories(test_core_settings PRIVATE wifi_store_stubs display_stubs ../../components/app_core/private ../../components/app_maintenance/include ../../components/app_wifi/include)
target_compile_options(test_core_settings PRIVATE -Wall -Wextra -Werror)
foreach(case normal legacy missing future open_failed read_failed set_failed commit_failed)
    add_test(NAME core_settings_${case} COMMAND test_core_settings ${case})
endforeach()

target_sources(test_ui_preferences PRIVATE ../../components/common_runtime/preferences_store.c ../support/legacy/preferences_set.c)

target_sources(test_core_factory_messages PRIVATE ../../components/app_core/app_core_mode.c)
add_executable(test_core_mode test_core_mode.c ../../components/app_core/app_core_mode.c)
target_include_directories(test_core_mode PRIVATE ../../components/app_core/private)
find_package(Threads REQUIRED)
target_link_libraries(test_core_mode PRIVATE Threads::Threads)
target_compile_options(test_core_mode PRIVATE -Wall -Wextra -Werror)
add_test(NAME core_mode COMMAND test_core_mode)
add_executable(test_ui_mode_messages test_ui_mode_messages.c ../../components/app_ui/ui_mode_messages.c)
target_include_directories(test_ui_mode_messages PRIVATE display_stubs ../../components/app_ui/private ../../components/app_console/include)
target_compile_options(test_ui_mode_messages PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_mode_messages COMMAND test_ui_mode_messages)
add_executable(test_core_maintenance test_core_maintenance.c ../../components/app_core/app_core_maintenance.c ../../components/app_core/app_core_shutdown.c ../../components/app_core/app_core_mode.c)
target_include_directories(test_core_maintenance PRIVATE core_start_stubs router_stubs maintenance_stubs display_stubs
    ../../components/app_core/include ../../components/app_core/private ../../components/app_maintenance/include
    ../../components/app_wifi/include ../../components/app_camera/include ../../components/app_console/include
    ../../components/app_input/include ../../components/app_ui/include)
target_compile_options(test_core_maintenance PRIVATE -Wall -Wextra -Werror)
foreach(case success boot_wait boot_timeout restart_commit_failed wifi_success wifi_failed wifi_pending normal http_stop U I P B C E N S J D R fixed config activate timeout config_stop ota_init web_init lifecycle_init)
    add_test(NAME core_maintenance_${case} COMMAND test_core_maintenance ${case})
endforeach()

add_executable(test_wifi_menu_current test_wifi_menu_current.c ../support/legacy/wifi_menu_noreset.c ../../common/network_config.c)
target_include_directories(test_wifi_menu_current PRIVATE ../support/legacy)
target_compile_options(test_wifi_menu_current PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_menu_current COMMAND test_wifi_menu_current)
add_executable(test_preferences_readonly test_preferences_readonly.c ../../components/app_ui/ui_preferences.c ../../components/app_console/ui_preferences_console.c)
target_include_directories(test_preferences_readonly PRIVATE display_stubs ../../common ../../components/app_ui/private ../../components/app_console/private ../../components/app_console/include ../../components/app_ui/include)
target_compile_options(test_preferences_readonly PRIVATE -Wall -Wextra -Werror)
add_test(NAME preferences_readonly COMMAND test_preferences_readonly)
add_executable(test_wifi_console_readonly test_wifi_console_readonly.c ../../components/app_console/wifi_console.c ../../common/network_config.c)
target_include_directories(test_wifi_console_readonly PRIVATE display_stubs ../../common ../../components/app_console/private ../../components/app_console/include)
target_compile_options(test_wifi_console_readonly PRIVATE -Wall -Wextra -Werror)
add_test(NAME wifi_console_readonly COMMAND test_wifi_console_readonly)

target_include_directories(test_ui_overlay BEFORE PRIVATE ../support/legacy)
add_executable(test_ui_jpeg_renderer test_ui_jpeg_renderer.c)
target_include_directories(test_ui_jpeg_renderer PRIVATE jpeg_stubs display_stubs
    ../../components/app_ui/include ../../components/app_ui/private ../../components/display_surface/include ../../common)
target_compile_definitions(test_ui_jpeg_renderer PRIVATE CONFIG_REMOTE_DBG_SIM=0)
set_property(TARGET test_ui_jpeg_renderer PROPERTY C_STANDARD 11)
target_compile_options(test_ui_jpeg_renderer PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_jpeg_renderer COMMAND test_ui_jpeg_renderer)
add_executable(test_ui_jpeg_renderer_debug test_ui_jpeg_renderer.c)
target_include_directories(test_ui_jpeg_renderer_debug PRIVATE jpeg_stubs display_stubs
    ../../components/app_ui/include ../../components/app_ui/private ../../components/display_surface/include ../../common)
target_compile_definitions(test_ui_jpeg_renderer_debug PRIVATE CONFIG_REMOTE_DBG_SIM=1)
set_property(TARGET test_ui_jpeg_renderer_debug PROPERTY C_STANDARD 11)
target_compile_options(test_ui_jpeg_renderer_debug PRIVATE -Wall -Wextra -Werror)
add_test(NAME ui_jpeg_renderer_debug COMMAND test_ui_jpeg_renderer_debug)

add_executable(test_wifi_esp32_backend test_wifi_esp32_backend.c ../../components/app_wifi/app_wifi.c ../../common/network_config.c ../../common/async_token.c)
target_include_directories(test_wifi_esp32_backend PRIVATE wifi_backend_stubs router_stubs display_stubs ../../components/wifi_esp32/include ../../components/wifi_esp32/private ../../components/app_wifi/include ../../common)
target_compile_options(test_wifi_esp32_backend PRIVATE -Wall -Wextra -Werror)
set_property(TARGET test_wifi_esp32_backend PROPERTY C_STANDARD 11)
add_test(NAME wifi_esp32_backend COMMAND test_wifi_esp32_backend)
