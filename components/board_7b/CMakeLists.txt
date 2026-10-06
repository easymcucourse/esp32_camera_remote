idf_component_register(SRCS "board_7b_backend.c" "board_lcd.c" INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private" PRIV_REQUIRES esp_lcd esp_driver_i2c esp_timer)
