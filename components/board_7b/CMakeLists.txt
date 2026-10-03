idf_component_register(SRCS "board_7b.c" "board_lcd.c" "ui_fonts.c" "camera_settings.c" INCLUDE_DIRS "include"
    REQUIRES esp_lcd PRIV_REQUIRES esp_driver_i2c esp_timer esp_rom esp_new_jpeg freetype
    EMBED_FILES "fonts/inter_ui.ttf" "fonts/han_ui.otf" "fonts/mono_ui.ttf")
target_sources(${COMPONENT_LIB} PRIVATE "../../common/ui_overlay.c")
target_include_directories(${COMPONENT_LIB} PRIVATE "../../common")
