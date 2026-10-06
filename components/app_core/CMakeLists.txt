# IDF resolves dependencies before sdkconfig is loaded. SIM stays registered
# as a config-only component in Release; its implementation sources are gated.
set(input_provider_dependencies app_input_atom app_input_sim)
idf_component_register(SRCS "app_core_start.c" "app_core_mode.c" "app_core_shutdown.c" "app_core_maintenance.c" "app_core_factory_reset.c" "app_core_settings.c" "app_core_ota_health.c" "app_core_wifi_boot.c" "app_health.c" "app_restart.c" "app_core_messages.c" "app_core_camera.c" "app_core_network.c" "app_core_input.c" "app_factory_transaction.c" INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "private" PRIV_REQUIRES app_wifi common_runtime app_maintenance app_update ${input_provider_dependencies} app_input app_camera app_console app_ui app_wifi_messages wifi_esp32 esp_psram nvs_flash esp_timer esp_system esp_hw_support freertos)
