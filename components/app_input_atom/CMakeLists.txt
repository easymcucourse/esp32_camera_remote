idf_component_register(SRCS "atom_link.c" "../../common/i2c_monitor.c" INCLUDE_DIRS "include"
    PRIV_REQUIRES app_input app_console atom_protocol esp_driver_i2c esp_timer freertos)
