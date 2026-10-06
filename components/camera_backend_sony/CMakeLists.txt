idf_component_register(SRCS "camera_backend_sony.c" "sony_props.c" "sony_liveview.c"
    "sony_control_encoder.c"
    PRIV_INCLUDE_DIRS "private" "../../common"
    PRIV_REQUIRES camera_backend ptpip)
