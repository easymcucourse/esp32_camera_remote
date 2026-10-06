idf_component_register(SRCS "ptpip_client.c" "ptpip_protocol.c" "ptp_wire.c" "ptp_dataset.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "private" REQUIRES app_console
    PRIV_REQUIRES esp_timer freertos)
