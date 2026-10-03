idf_component_register(SRCS "app_main.c" "atom_console.c" "pad_console.c" "../../common/debug_args.c" "../../common/debug_line.c" "../../common/debug_console.c" "atom_i2c.c" "atom_slave_tx.c" "matrix_status.c" "matrix_model.c" "ds4_host.c" "ds4_report.c" "ds4_events.c" "../../common/atom_protocol.c"
    INCLUDE_DIRS "." "../../common"
    PRIV_REQUIRES driver esp_driver_i2c esp_driver_gpio esp_driver_rmt bt nvs_flash esp_driver_uart esp_app_format)
target_sources(${COMPONENT_LIB} PRIVATE "../../common/i2c_monitor.c" "../../common/i2c_debug.c")
if(CONFIG_REMOTE_DBG_SIM)
    target_sources(${COMPONENT_LIB} PRIVATE "../../common/pad_cmd.c" "../../common/pad_player.c" "../../common/atom_fault.c" "../../common/debug_hex.c" "matrix_debug.c")
endif()
# IDF 5.5.1-specific bounded TX replacement; see atom_slave_tx.c.
idf_component_get_property(i2c_driver_dir esp_driver_i2c COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${i2c_driver_dir}")
