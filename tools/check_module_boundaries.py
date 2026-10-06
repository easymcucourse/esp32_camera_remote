"""Check boundaries established by the staged main-module migration."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def storage_writer_issues(root):
    """Direct LCD NVS writes belong to one primitive per namespace.

    This lexical gate supplements owner/phase review, not lock or SMP proof.
    ATOM is a separate firmware with its own persistent records.
    """
    owners = {
        'components/wifi_esp32/wifi_saved_config.c',
        'components/common_runtime/camera_identity_store.c',
        'components/common_runtime/preferences_store.c',
    }
    issues = []
    for directory in ('main', 'components', 'common'):
        for source in sorted((root / directory).rglob('*.c')):
            relative = source.relative_to(root).as_posix()
            text = source.read_text(encoding='utf-8')
            text = re.sub(r'/\*.*?\*/|//[^\n]*', '', text, flags=re.DOTALL)
            if re.search(r'\bnvs_flash_erase(?:_partition)?\s*\(', text):
                issues.append(f'{relative}: global NVS erase bypasses namespace owners')
            if relative not in owners and re.search(r'\bnvs_(?:set_\w+|erase_\w+|commit)\s*\(', text):
                issues.append(f'{relative}: direct NVS write outside namespace primitive')
    return issues


def check(root=ROOT):
    issues = storage_writer_issues(root)
    public_versions = {
        'app_core/include/app_core.h': 'APP_CORE_API_VERSION',
        'app_console/include/app_console.h': 'APP_CONSOLE_API_VERSION',
        'app_console/include/app_message.h': 'APP_MESSAGE_API_VERSION',
        'app_camera/include/app_camera.h': 'APP_CAMERA_API_VERSION',
        'app_ui/include/app_ui.h': 'APP_UI_API_VERSION',
        'app_wifi/include/app_wifi.h': 'APP_WIFI_API_VERSION',
        'app_wifi_messages/include/app_wifi_messages.h': 'APP_WIFI_MESSAGES_API_VERSION',
        'app_input/include/app_input.h': 'APP_INPUT_API_VERSION',
        'app_input/include/input_provider.h': 'INPUT_PROVIDER_API_VERSION',
        'app_input_atom/include/input_atom.h': 'APP_INPUT_ATOM_API_VERSION',
        'app_input_sim/include/input_sim.h': 'APP_INPUT_SIM_API_VERSION',
        'app_maintenance/include/app_maintenance.h': 'APP_MAINTENANCE_API_VERSION',
        'app_maintenance/include/app_maintenance_web.h': 'APP_MAINTENANCE_WEB_API_VERSION',
        'app_maintenance/include/app_maintenance_ota.h': 'APP_MAINTENANCE_OTA_API_VERSION',
    }
    for relative, macro in public_versions.items():
        header = root/'components'/relative
        if not header.exists() or not re.search(
                rf'^\s*#\s*define\s+{macro}\s+[1-9][0-9]*[uU]?\b',
                header.read_text(encoding='utf-8'), re.MULTILINE):
            issues.append(f'components/{relative}: missing positive public API version {macro}')
    core_public=root/'components/app_core/include'
    if core_public.exists():
        if {p.name for p in core_public.glob('*.h')} != {'app_core.h'}:
            issues.append('components/app_core/include: internal services exported')
        header=core_public/'app_core.h'
        if header.exists():
            calls=set(re.findall(r'\b(app_\w+)\s*\(',header.read_text(encoding='utf-8')))
            if calls != {'app_core_start'}:
                issues.append('components/app_core/include/app_core.h: non-startup API exported')
    ui_public=root/'components/app_ui/include'
    if ui_public.exists() and {p.name for p in ui_public.glob('*.h')} != {'app_ui.h'}:
        issues.append('components/app_ui/include: model/renderer implementation header exported')
    entry=root/'main/app_main.c'
    if entry.exists():
        text=entry.read_text(encoding='utf-8')
        includes=set(re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]',text,re.MULTILINE))
        if includes != {'app_core.h'} or not re.search(r'ESP_ERROR_CHECK\s*\(\s*app_core_start\s*\(\s*\)\s*\)',text):
            issues.append('main/app_main.c: entry bypasses Core composition')
        if re.search(r'\b(?:app_ui_|wifi_ap_|maint_|app_input_|camera_)\w*\s*\(',text):
            issues.append('main/app_main.c: service composition remains in ESP-IDF entry')
    if (root/'main/wifi_ap.c').exists():
        issues.append('main/wifi_ap.c: Wi-Fi object composition remains outside Core')
    for obsolete in ('app_core_wifi_compat.c', 'private/app_core_wifi_compat.h'):
        if (root/'components/app_core'/obsolete).exists():
            issues.append(f'components/app_core/{obsolete}: retired Wi-Fi compatibility layer remains')
    wifi_boot=root/'components/app_core/app_core_wifi_boot.c'
    if wifi_boot.exists() and re.search(r'\bapp_wifi_saved_config_(?:write|reset)\s*\(', wifi_boot.read_text(encoding='utf-8')):
        issues.append('components/app_core/app_core_wifi_boot.c: startup persists user Wi-Fi configuration')
    for name in ('network_config.c', 'network_config.h'):
        source = root / 'common' / name
        if source.exists():
            headers = set(re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]', source.read_text(encoding='utf-8'), re.MULTILINE))
            if headers - {'network_config.h', 'string.h', 'stdbool.h', 'stddef.h', 'stdint.h'}:
                issues.append(f'common/{name}: shared config value kernel has implementation dependencies')
    console_cmake=root/'components/app_console/CMakeLists.txt'
    if console_cmake.exists():
        words=set(re.findall(r'\b[a-z][a-z0-9_]*\b',console_cmake.read_text(encoding='utf-8')))
        if words & {'app_camera','app_ui','app_input','app_input_atom','app_input_sim','app_wifi','app_wifi_messages','app_core','camera_backend','camera_backend_sony','wifi_esp32','board_7b','display_surface','ptpip','sony_camera'}:
            issues.append('components/app_console: gateway/router depends on functional component')
    for name in ('camera_console','wifi_console','camera_commands','uart_status','i2c_console','lcd_sim','display_bench','ui_preferences_console','input_console','maint_probe'):
        for extension in ('.c','.h'):
            if (root/'main'/(name+extension)).exists():
                issues.append(f'main/{name}{extension}: UART gateway or removed maintenance probe remains in main')
    legacy_headers = {'sony_ext.h', 'ptp_session.h', 'ptpip_transport.h'}
    legacy_sources = {'sony_ext.c', 'ptp_session.c', 'ptp_send_data.c', 'ptpip_transport.c'}
    for obsolete in ('sony_client_controls.c', 'private/sony_client_controls.h'):
        if (root/'components/camera_backend_sony'/obsolete).exists():
            issues.append(f'components/camera_backend_sony/{obsolete}: redundant control forwarding layer remains')
    for component in ('ptpip', 'camera_backend_sony'):
        cmake = root / 'components' / component / 'CMakeLists.txt'
        if cmake.exists() and re.search(r'\b(?:lwip|sony_camera)\b', cmake.read_text(encoding='utf-8')):
            issues.append(f'{cmake.relative_to(root)}: obsolete production dependency')
    maintenance_cmake = root / 'components/app_maintenance/CMakeLists.txt'
    for obsolete in ('maint_mode.c','maint_mode.h','maint_confirm.h','maint_notice.h','camera_pair.h','ui_preferences.h','wifi_ap.h'):
        if (root/'main'/obsolete).exists():
            issues.append(f'main/{obsolete}: obsolete maintenance controller remains in firmware')
    main_cmake=root/'main/CMakeLists.txt'
    if main_cmake.exists() and 'WHOLE_ARCHIVE' in main_cmake.read_text(encoding='utf-8'):
        issues.append('main/CMakeLists.txt: obsolete maintenance archive workaround')
    if maintenance_cmake.exists() and re.search(r'\b(?:app_core|app_camera|app_ui|app_console|app_input\w*|app_wifi_messages|wifi_esp32)\b',maintenance_cmake.read_text(encoding='utf-8')):
        issues.append('components/app_maintenance: normal application dependency')
    sony_cmake = root / 'components/sony_camera/CMakeLists.txt'
    if sony_cmake.exists():
        issues.append('components/sony_camera: obsolete standalone component')
    for directory in (root / 'main', root / 'components'):
        for source in sorted(directory.rglob('*')):
            if source.suffix not in ('.c', '.h'):
                continue
            relative = source.relative_to(root).as_posix()
            component = relative.split('/')[1] if relative.startswith('components/') else 'main'
            if re.search(r'\bsony_encode_set_exposure_mode\b', source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: test-only Sony exposure wrapper remains in production')
            if re.search(r'\b(?:app_ui_set_camera_property|app_ui_set_command_status|camera_extra_codes)\b',
                         source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: retired vendor-code UI interface remains in production')
            includes = set(re.findall(r'^\s*#\s*include\s*[<"]([^">]+)[">]',
                                      source.read_text(encoding='utf-8'), re.MULTILINE))
            if re.search(r'\bmaint_mode_\w+\s*\(',source.read_text(encoding='utf-8')) or 'app_core_maintenance_compat.h' in includes:
                issues.append(f'{relative}: obsolete maintenance controller dependency')
            if source.name in ('maint_auth.c','maint_auth.h') or 'maint_auth.h' in includes:
                issues.append(f'{relative}: obsolete maintenance authentication in firmware')
            if component == 'app_maintenance':
                for header in includes:
                    if header.startswith(('app_core', 'app_camera', 'app_console', 'app_message', 'app_input', 'app_ui', 'camera_', 'ui_', 'wifi_ap', 'maint_mode', 'app_restart', 'debug_console')):
                        issues.append(f'{relative}: maintenance depends on normal application {header}')
                if re.search(r'\b(?:camera_\w+|app_ui_\w+|app_console_\w+|maint_mode_\w+|app_restart_\w+)\s*\(',source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: maintenance invokes normal application')
            if 'wifi_http_scope.h' in includes and component != 'wifi_esp32':
                issues.append(f'{relative}: application exposes SDK HTTP socket hook')
            if component == 'sony_camera' or source.name in legacy_sources:
                issues.append(f'{relative}: legacy Sony/fd implementation remains in production')
            for header in includes:
                backend_header = Path(header).name
                if (backend_header.startswith(('ptp_', 'ptpip_', 'sony_', 'camera_backend')) and
                        component not in ('app_camera', 'camera_backend', 'camera_backend_sony', 'ptpip')):
                    issues.append(f'{relative}: private camera backend/protocol boundary {header}')
                if Path(header).name in legacy_headers:
                    issues.append(f'{relative}: legacy Sony/fd API {header}')
            if 'liveview_pipeline.h' in includes:
                issues.append(f'{relative}: legacy JPEG worker remains in production')
            if 'app_camera.h' in includes and component not in ('app_core','app_camera'):
                issues.append(f'{relative}: Camera lifecycle facade outside Core')
            if 'app_input.h' in includes and component not in ('app_core','app_input'):
                issues.append(f'{relative}: Input lifecycle facade outside Core')
            for header,owner in (('input_atom.h','app_input_atom'),('input_sim.h','app_input_sim')):
                if header in includes and component not in ('app_core',owner):
                    issues.append(f'{relative}: provider lifecycle outside Core')
            if component in ('app_input_atom','app_input_sim'):
                for header in includes:
                    if header.startswith(('camera_', 'app_camera', 'app_ui', 'ui_', 'maint_', 'gamepad_input', 'debug_console', 'wifi_', 'app_wifi', 'nvs')):
                        issues.append(f'{relative}: provider depends on business/UART implementation {header}')
                if re.search(r'\b(?:gamepad_input_\w+|camera_gamepad_\w+|maint_mode_gamepad|debug_printf)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: provider interprets business or UART commands')
            if component in ('main','app_console') and source.name in ('atom_link.c','atom_link.h'):
                issues.append(f'{relative}: physical provider belongs to app_input_atom')
            if component in ('main','app_console') and source.name=='lcd_sim.c':
                if includes & {'atom_sim.h','atom_client.h','pad_player.h','input_provider.h'}:
                    issues.append(f'{relative}: UART encoder owns SIM/provider state')
            if component in ('main','app_console') and source.name=='wifi_console.c':
                if includes & {'wifi_ap.h','app_wifi.h','app_core.h','wifi_esp32.h','nvs.h','esp_wifi.h'}:
                    issues.append(f'{relative}: Wi-Fi UART encoder bypasses typed messages')
                if re.search(r'\b(?:wifi_ap_\w+|app_wifi_\w+|app_core_factory_\w+)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: Wi-Fi UART encoder calls domain implementation')
            if component in ('main','app_console') and source.name=='ui_preferences_console.c':
                if includes & {'ui_preferences.h','ui_overlay.h','app_ui.h','atom_protocol.h','nvs.h'}:
                    issues.append(f'{relative}: preference UART encoder depends on UI/provider implementation')
                if re.search(r'\bui_preferences_(?:level|pad|request|result|set_pad)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: preference UART encoder bypasses typed messages')
            if component in ('main','app_console') and source.name in ('camera_commands.c','uart_status.c','camera_console.c'):
                if includes & {'camera_pair.h','app_camera.h','app_ui.h','camera_settings.h','ui_preferences.h','ui_overlay.h','maint_ota.h'}:
                    issues.append(f'{relative}: camera/status UART encoder depends on functional implementation')
                if re.search(r'\b(?:camera_(?:pair_start|jpeg_start|stop_request|forget_pairing|focus_cancel|debug_get_status|gamepad_caps)|app_ui_\w+)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: camera/status UART encoder bypasses typed messages')
            if component in ('main','app_console') and source.name=='display_bench.c':
                if includes & {'app_ui.h','camera_pair.h','maint_mode.h','lcd_sim.h','input_console.h','esp_heap_caps.h'}:
                    issues.append(f'{relative}: benchmark UART encoder owns functional/resources implementation')
                if re.search(r'\b(?:app_ui_\w+|camera_\w+|maint_mode_\w+|heap_caps_\w+|xTaskCreate\w*)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: benchmark UART encoder bypasses typed messages')
            if component == 'main' and source.name == 'camera_controller.c':
                for header in includes:
                    if header.startswith(('sony_', 'ptp', 'wifi_', 'app_wifi', 'lwip/')):
                        issues.append(f'{relative}: Camera producer bypasses generic backend {header}')
                if re.search(r'\b(?:ptpip_\w+|ptp_\w+|sony_\w+|wifi_ap_\w+|socket|recv|send|select|close|app_ui_show_\w+|app_ui_set_\w+)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: Camera producer calls protocol/network/renderer directly')
            if component == 'main' and source.name == 'atom_link.c':
                for header in includes:
                    if header.startswith(('camera_', 'app_camera', 'app_ui', 'ui_', 'maint_', 'gamepad_input')):
                        issues.append(f'{relative}: provider depends on business implementation {header}')
                if re.search(r'\b(?:gamepad_input_\w+|camera_gamepad_\w+|maint_mode_gamepad)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: provider interprets business actions')
                if re.search(r'\bapp_ui_set_(?:sim|atom_status|atom_protocol|controller_battery)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: input state bypasses typed UI message')
                if re.search(r'\bapp_ui_(?:toggle_settings_mode|menu_move|extra_menu_open|extra_menu_move)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: input navigation bypasses typed UI message')
            if component == 'main' and source.name in ('wifi_menu.c', 'wifi_menu.h', 'wifi_menu_ui.c', 'wifi_menu_ui.h'):
                issues.append(f'{relative}: Wi-Fi menu belongs to UI')
            if source.name == 'wifi_config.c':
                issues.append(f'{relative}: shared config value kernel belongs to common')
            if component in ('main', 'app_camera') and source.name in ('camera_properties.c', 'camera_properties.h',
                    'camera_menu.c', 'camera_menu.h', 'setting_control.c', 'setting_control.h'):
                for header in includes:
                    if header.startswith(('sony_', 'ptp', 'app_ui', 'app_wifi', 'wifi_', 'maint_', 'lwip/')):
                        issues.append(f'{relative}: camera property kernel dependency {header}')
            if component == 'app_camera' and source.name in ('camera_settings_execute.c', 'camera_settings_execute.h'):
                for header in includes:
                    if header.startswith(('app_', 'sony_', 'ptp', 'wifi_', 'maint_', 'lwip/', 'freertos/')):
                        issues.append(f'{relative}: camera setting executor dependency {header}')
            if component == 'app_camera':
                for header in includes:
                    selected_factory = source.name == 'camera_backend_binding.c' and header == 'camera_backend_sony_factory.h'
                    if not selected_factory and header.startswith(('sony_', 'ptp', 'camera_backend_sony', 'app_wifi', 'wifi_', 'app_ui', 'maint_', 'lwip/')):
                        issues.append(f'{relative}: Camera domain implementation dependency {header}')
                    if header in ('gamepad_input.h', 'atom_link.h', 'camera_controller.h'):
                        issues.append(f'{relative}: Camera dependency on old input/controller implementation {header}')
            if component == 'app_ui':
                if re.search(r'\b(?:app_ui_(?:set_maint_text|request_maint_screen|set_maint_menu|set_wifi_menu)|ui_render_draw_maintenance_notice|app_ui_wifi_menu_view_t)\b',source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: retired normal maintenance/Wi-Fi editor UI remains')
                for header in includes:
                    if header.startswith(('sony_', 'ptp', 'camera_backend', 'app_camera', 'wifi_', 'app_wifi', 'maint_', 'input_', 'lwip/')):
                        issues.append(f'{relative}: UI domain implementation dependency {header}')
                    if header in ('debug_console.h', 'atom_link.h', 'atom_client.h', 'atom_protocol.h'):
                        issues.append(f'{relative}: UI depends on UART/provider implementation {header}')
            if component != 'app_core' and includes & {'app_core_services.h','app_core_factory.h','app_restart.h'}:
                issues.append(f'{relative}: Core private composition/transaction header crosses component boundary')
            if component != 'app_ui' and 'app_ui_internal.h' in includes:
                issues.append(f'{relative}: UI private model/renderer contract crosses component boundary')
            if component == 'app_camera' and re.search(r'\b(?:app_camera_quiesce(?:_release)?|app_camera_messages_stop|camera_controller_forget|camera_forget_pairing|camera_maintenance_acquire|camera_maintenance_release)\b',source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: retired Camera reservation/forget API remains')
            if component == 'app_input':
                for header in includes:
                    if header.startswith(('camera_', 'app_camera', 'app_ui', 'sony_', 'ptp', 'wifi_', 'app_wifi', 'maint_', 'lwip/', 'driver/')):
                        issues.append(f'{relative}: input kernel depends on domain/device implementation {header}')
            if component == 'camera_backend':
                for header in includes:
                    if header not in ('camera_backend.h', 'stdbool.h', 'stddef.h', 'stdint.h'):
                        issues.append(f'{relative}: generic camera backend dependency {header}')
            if component == 'camera_backend_sony':
                for header in includes:
                    if header.startswith(('lwip/', 'app_wifi', 'app_ui', 'maint_', 'input_')) or header in (
                            'esp_wifi.h', 'esp_netif.h', 'ptpip_transport.h', 'ptp_session.h', 'camera_controller.h'):
                        issues.append(f'{relative}: Sony backend bypasses PTP instance boundary {header}')
            if component == 'ptpip':
                for header in includes:
                    if header in ('app_wifi.h', 'esp_wifi.h', 'esp_netif.h', 'sys/socket.h', 'sys/select.h') or header.startswith('lwip/'):
                        issues.append(f'{relative}: PTP client bypasses message boundary {header}')
            if component != 'wifi_esp32' and includes & {'esp_wifi.h', 'esp_netif.h', 'lwip/sockets.h', 'sys/socket.h', 'sys/select.h'}:
                issues.append(f'{relative}: network implementation header outside Wi-Fi backend')
            if component == 'app_maintenance':
                if any(header.startswith('lwip/') or header in ('esp_wifi.h', 'esp_netif.h', 'sys/socket.h') for header in includes):
                    issues.append(f'{relative}: maintenance bypasses Wi-Fi/SDK HTTP transport boundary')
                if re.search(r'\b(?:socket|bind|listen|accept|recv|send|select|shutdown|setsockopt|getsockopt|lwip_\w+)\s*\(', source.read_text(encoding='utf-8')):
                    issues.append(f'{relative}: raw maintenance socket operation')
            if component != 'wifi_esp32' and re.search(r'\bnvs_open\s*\(\s*"wifi_ap"', source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: Wi-Fi NVS access outside backend')
            if component != 'common_runtime' and re.search(r'\bnvs_open\s*\(\s*"ui_prefs"', source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: preference NVS access outside shared storage owner')
            if source.name=='app_factory_service.c' or re.search(r'\bapp_core_factory_(?:start|request|result|quiesce)\s*\(',source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: normal factory worker remains in production')
            if source.suffix=='.c' and re.search(r'\bAPP_MESSAGE_SYSTEM_FACTORY_(?:RESET|RESULT)\b',source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: normal factory message path remains in production')
            if source.name in ('wifi_menu.c','wifi_menu_ui.c','ui_wifi_menu.h','ui_wifi_menu_kernel.h'):
                issues.append(f'{relative}: normal Wi-Fi editor remains in production')
            if source.suffix=='.c' and component in ('app_ui','app_console','app_wifi_messages') and re.search(r'\b(?:app_wifi_config_(?:apply|commit|cancel)|APP_MESSAGE_WIFI_CONFIG_(?:PREPARE|COMMIT|CANCEL|RESULT))\b',source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: normal application configuration write path')
            if component in ('app_ui','app_input','app_console') and re.search(r'\b(?:preferences_store_(?:set|write|reset)|ui_preferences_(?:request|set_pad))\s*\(',source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: normal application persists UI preferences')
            if component != 'common_runtime' and re.search(r'\bnvs_open\s*\(\s*"sony_remote"', source.read_text(encoding='utf-8')):
                issues.append(f'{relative}: pairing NVS access outside shared storage owner')
            for header in ('board_7b.h', 'board_lcd.h', 'board_dimensions.h', 'display_backend.h'):
                allowed = component == 'board_7b' or (header == 'display_backend.h' and component == 'display_surface')
                if header in includes and not allowed:
                    issues.append(f'{relative}: hardware include {header}')
            if 'display_surface.h' in includes and component not in ('app_ui', 'display_surface'):
                issues.append(f'{relative}: canvas access outside app_ui')
            if component == 'board_7b':
                for header in includes:
                    if header.startswith(('app_', 'camera_', 'sony_', 'ptp', 'ui_')):
                        issues.append(f'{relative}: application dependency {header}')
            if component == 'app_console':
                for header in includes:
                    if header.startswith(('camera_', 'wifi_', 'sony_', 'ptp', 'maint_', 'input_')) or (
                            header.startswith('app_') and header not in (
                                'app_console.h', 'app_message.h', 'app_message_internal.h')):
                        issues.append(f'{relative}: router dependency on functional header {header}')
            if component in ('app_wifi', 'app_wifi_messages'):
                for header in includes:
                    if header in ('esp_wifi.h', 'esp_netif.h', 'wifi_esp32.h') or header.startswith('lwip/'):
                        issues.append(f'{relative}: Wi-Fi implementation dependency {header}')
                    if header.startswith(('camera_', 'sony_', 'ptp', 'maint_', 'app_ui', 'input_')):
                        issues.append(f'{relative}: Wi-Fi dependency on business header {header}')
    return issues


if __name__ == '__main__':
    issues = check()
    if issues:
        print('\n'.join(issues))
        sys.exit(1)
    print('Established module boundaries passed')
