idf_component_register(SRCS "ui_renderer.c" "ui_render_lifecycle.c" "ui_jpeg_renderer.c" "ui_model.c" "ui_message_endpoint.c" "ui_camera_messages.c" "ui_input_messages.c" "ui_menu_messages.c" "ui_preferences.c" "ui_frames.c" "ui_mode_messages.c" "ui_fonts.c" "camera_settings.c" "../../common/ui_overlay.c"
    INCLUDE_DIRS "include" PRIV_INCLUDE_DIRS "private" "../../common"
    REQUIRES app_console PRIV_REQUIRES common_runtime freertos display_surface esp_timer esp_rom esp_new_jpeg freetype
    EMBED_FILES "fonts/inter_ui.ttf" "fonts/han_ui.otf" "fonts/mono_ui.ttf")
if(CONFIG_REMOTE_DBG_SIM)
    target_sources(${COMPONENT_LIB} PRIVATE "ui_bench.c")
endif()
