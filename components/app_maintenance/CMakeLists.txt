idf_component_register(SRCS "maintenance_ota.c" "ota_header.c" "maintenance_trigger.c" "maintenance_web.c" "maint_json.c" "maint_wifi.c" INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private" REQUIRES app_wifi esp_http_server PRIV_REQUIRES common_runtime esp_timer esp_hw_support app_update esp_app_format heap json freertos)

execute_process(COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/../../tools/pack_web.py"
    "${CMAKE_CURRENT_LIST_DIR}/web/index.html" "${CMAKE_CURRENT_BINARY_DIR}/index.html.gz"
    RESULT_VARIABLE web_pack_result)
if(NOT web_pack_result EQUAL 0)
    message(FATAL_ERROR "Cannot compress maintenance page")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "web/index.html" "../../tools/pack_web.py")
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_BINARY_DIR}/index.html.gz" BINARY)
