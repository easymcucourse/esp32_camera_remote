cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_camera_remote)
# ESP-IDF 5.5 HTTP has no bind-address option. Scope only that SDK transport's
# bind primitive through the chosen Wi-Fi backend; all other socket users keep
# native lwIP. No SDK files are edited, and the separate ATOM project is untouched.
if(TARGET __idf_esp_http_server AND TARGET __idf_wifi_esp32)
    target_compile_definitions(__idf_esp_http_server PRIVATE lwip_bind=wifi_esp32_http_bind)
    target_link_libraries(__idf_esp_http_server INTERFACE __idf_wifi_esp32)
    # Record the actual post-project link interface; project_description.json
    # is emitted before this edge exists and cannot prove the complete graph.
    file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/module-http-links.txt"
        CONTENT "$<TARGET_PROPERTY:__idf_esp_http_server,INTERFACE_LINK_LIBRARIES>\n")
endif()
