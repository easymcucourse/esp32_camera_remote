#include "atom_console.h"
#include "debug_console.h"
#include "ds4_host.h"
#include "atom_i2c.h"
#include "matrix_status.h"
#include "pad_console.h"
#include "ble_gamepad.h"
#include "gimbal_link.h"
#include "i2c_debug.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>

static bool command(int argc, char **argv)
{
    if (gimbal_link_command(argc,argv)) return true;
    if (ble_gamepad_command(argc,argv)) return true;
    if (i2c_debug_command(argc,argv)) return true;
    if (atom_i2c_debug_command(argc,argv)) return true;
    if (pad_console_command(argc,argv)) return true;
    if (matrix_status_debug_command(argc,argv)) return true;
    if (argc != 1) return false;
    if (!strcmp(argv[0], "help")) {
        debug_printf("[dbg] OK help: version; status; log <tag|*> <none|error|warn|info|debug|verbose>; i2c log on|off|changes; i2c stats [reset]\n");
        debug_printf("[dbg] ble map: cached HID report descriptor (no device identity)\n");
        debug_printf("[dbg] gimbal status|on|off|pair|stop|calibrate|speed [pan|tilt] 20..400|invert 0|1\n");
#if CONFIG_REMOTE_DBG_SIM
        debug_printf("[dbg] SIM i2c: req <9 hex bytes>; drop 0..10000; corrupt 0..10000; delay 0..200 (milliseconds)\n");
        debug_printf("[dbg] SIM led: test (toggle corners every 1s); off; fault bt|i2c|overflow on|off\n");
        debug_printf("[dbg] SIM help: pad sim on|off; pad connect|disconnect; pad battery 0..10|none; pad overflow; tap; hold; release; stick; trigger; shoot; record; seq\n");
#endif
        return true;
    }
    if (strcmp(argv[0], "status")) return false;
    ds4_state_t pad; uint8_t link; unsigned queued; uint32_t dropped, age, invalid;
    ds4_host_debug_status(&pad, &link, &queued, &dropped);
    atom_i2c_get_status(&age, &invalid);
    matrix_model_t matrix; matrix_status_get_state(&matrix);
    bool calibration; uint8_t forced; matrix_status_debug_get(&calibration,&forced);
    debug_printf("[dbg] OK status lcd=%d lcd_age_ms=%lu invalid=%lu ds4=%u buttons=0x%05lx L=(%d,%d) R=(%d,%d) LT=%u RT=%u battery=%u queued=%u dropped=%lu ble=%u gimbal=%u matrix_boot=%u faults=0x%02x free_internal=%u sim=%d\n",
        atom_i2c_online(), (unsigned long)age, (unsigned long)invalid, link,
        (unsigned long)pad.buttons, pad.lx, pad.ly, pad.rx, pad.ry, pad.l2, pad.r2, pad.battery,
        queued, (unsigned long)dropped, matrix.ble_pad, matrix.gimbal, matrix.boot,
        matrix.faults, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),ds4_host_sim_active());
    debug_printf("[dbg] led calibration=%d forced=0x%02x%s\n",calibration,forced,calibration || forced?" SIM":"");
    debug_printf("[dbg] matrix battery ds=%u ble=%u gimbal=%u (255=unknown)\n",
                 matrix.classic_battery,matrix.ble_battery,matrix.gimbal_battery);
    return true;
}
static void poll(void) { pad_console_poll(); atom_i2c_debug_poll(); i2c_debug_poll(); }
void atom_console_start(void) { ESP_ERROR_CHECK(pad_console_start()); ESP_ERROR_CHECK(debug_console_start(command, poll)); }
