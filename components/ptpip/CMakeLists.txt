idf_component_register(SRCS "ptpip_transport.c" "ptp_session.c" "ptp_dataset.c"
    INCLUDE_DIRS "include" PRIV_REQUIRES lwip log esp_timer)
