#include "camera_console.h"
#include "camera_pair.h"
#include "board_7b.h"
#include "wifi_console.h"
#include "debug_console.h"
#include "atom_link.h"
#include "i2c_debug.h"
#include "lcd_sim.h"
#include "ui_preferences.h"
#include "ui_overlay.h"
#include "maint_mode.h"
#include "maint_probe.h"
#include "display_bench.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>

static bool command(int argc, char **argv)
{
    if (display_bench_command(argc,argv)) return true;
    if (i2c_debug_command(argc,argv)) return true;
    if (maint_probe_command(argc,argv)) return true;
    if (maint_mode_command(argc,argv)) return true;
    if (ui_preferences_command(argc,argv)) return true;
    if (lcd_sim_command(argc,argv)) return true;
    if (wifi_console_command(argc, argv)) return true;
    if (argc == 1 && !strcmp(argv[0], "help")) {
        debug_printf("[dbg] OK help: version; status; log <tag|*> <level>; i2c log on|off|changes; i2c stats [reset]; display fault off|once|persistent; j / s / S / p / u; wifi show [password]; wifi set <field> <value>...; wifi display on|off; wifi newpass; factory wifi|all|confirm\n");
        debug_printf("[dbg] ui: info [full|compact|hidden|next] (saved); DS4 touchpad cycles LIVE information\n");
        debug_printf("[dbg] maint: on [stop]|off|status; connection Share hold 2s; SETTINGS MAINTENANCE double A; web uses LCD PIN\n");
#if CONFIG_REMOTE_DBG_SIM
        debug_printf("[dbg] display bench: synthetic JPEG decode/overlay/publish profile; pauses camera and restores it\n");
        debug_printf("[dbg] SIM help: atom sim on|off; atom online|offline|reboot; atom version; atom fail|crc|timeout; pad connect|disconnect|gap|overflow; pad battery; gimbal state; tap; hold; release; stick; trigger; shoot; record; seq\n");
#endif
        return true;
    }
    if (argc == 1 && !strcmp(argv[0], "status")) {
        debug_printf("[dbg] menu selected=%u maint=%d\n",board_7b_menu_selected(),maint_mode_is_on());
        atom_link_status_t atom; atom_link_get_status(&atom);
        camera_debug_status_t camera; camera_debug_get_status(&camera);
        board_status_t board; board_7b_get_status(&board);
        debug_printf("[dbg] OK status camera_busy=%d session=%d stopped=%d last_io=%d phase=\"%s\" fps=%u.%u camera_battery=%u focus=0x%04x settings=%d display_failed=%d atom=%d protocol=2 mismatch=%d boot_id=%lu failures=%u ack_id=%lu ds4=%d buttons=0x%05lx R=(%d,%d) LT=%u RT=%u free_internal=%u free_psram=%u sim=%d left_stick=not_forwarded\n",
            camera.busy, camera.session, camera.stopped, camera.last_io, camera.phase,
            board.fps_tenths / 10, board.fps_tenths % 10, board.battery, board.focus, board.settings, board.failed,
            atom.client.online, atom.client.mismatch, (unsigned long)atom.client.boot_id,
            atom.client.failures, (unsigned long)atom.client.ack_id, atom.pad.connected,
            (unsigned long)atom.pad.buttons, atom.pad.rx, atom.pad.ry, atom.pad.lt, atom.pad.rt,
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),atom.sim);
        debug_printf("[dbg] wifi default_password=%d\n",board.default_password);
        debug_printf("[dbg] ui info=%s\n",ui_info_name(ui_preferences_level()));
        debug_printf("[dbg] heap min_internal=%u min_psram=%u largest_internal=%u largest_psram=%u\n",
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
        return true;
    }
    if (!strcmp(argv[0], "display")) {
        if (argc != 3 || strcmp(argv[1], "fault")) {
            debug_printf("[dbg] ERR usage: display fault off|once|persistent\n"); return true;
        }
        unsigned mode = !strcmp(argv[2], "off") ? 0 : !strcmp(argv[2], "once") ? 1 :
                        !strcmp(argv[2], "persistent") ? 2 : 3;
        if (mode == 3) { debug_printf("[dbg] ERR usage: display fault off|once|persistent\n"); return true; }
        esp_err_t err = board_7b_test_display_fault(mode);
        if (err == ESP_OK) debug_printf("[dbg] OK display fault %s (RAM only)\n", argv[2]);
        else debug_printf("[dbg] ERR display fault %s\n", esp_err_to_name(err));
        return true;
    }
    if (argc != 1 || strlen(argv[0]) != 1 || !strchr("pPjJuSs", argv[0][0])) return false;
    char key = argv[0][0];
    if (key == 'p' || key == 'P') camera_pair_start();
    if (key == 'j' || key == 'J') camera_jpeg_start();
    if (key == 'u' && !camera_forget_pairing()) {
        debug_printf("[dbg] ERR u camera busy or identity erase failed\n"); return true;
    }
    if (key == 'S') { board_7b_toggle_settings_mode(); camera_focus_cancel(); }
    if (key == 's') camera_stop_request();
    debug_printf("[dbg] OK %c\n", key);
    return true;
}

static void poll(void) { wifi_console_poll(); lcd_sim_poll(); ui_preferences_poll(); i2c_debug_poll(); maint_mode_poll(); maint_probe_poll(); display_bench_poll(); }
void camera_console_init(void)
{
    ESP_ERROR_CHECK(debug_console_start(command, poll));
    ESP_LOGI("camera_pair", "UART line console: help, version, status, log, display, j, S, s, p, u, wifi, factory; press Enter");
}
