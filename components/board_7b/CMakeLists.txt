idf_component_register(SRCS "board_7b.c" INCLUDE_DIRS "include"
    REQUIRES esp_lcd PRIV_REQUIRES esp_driver_i2c esp_timer esp_rom esp_new_jpeg)
