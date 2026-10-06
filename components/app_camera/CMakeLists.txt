idf_component_register(SRCS "camera_session.c" "camera_backend_binding.c"
    "camera_properties.c" "camera_menu.c" "setting_control.c" "camera_link.c" "camera_discovery.c" "camera_outputs.c" "camera_actions.c" "camera_identity_work.c" "camera_controls.c" "camera_frames.c" "camera_settings_execute.c" "camera_stream.c" "camera_runtime.c" "camera_endpoint.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private" "../../common" "../camera_backend_sony/private"
    PRIV_REQUIRES camera_backend camera_backend_sony app_console common_runtime esp_timer)
