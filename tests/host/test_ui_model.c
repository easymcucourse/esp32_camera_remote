#include "../support/legacy/ui_camera_vendor_codes.h"
#include "app_ui_internal.h"
#include "camera_settings.h"
#include "ui_model.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static unsigned notifications;
void app_ui_refresh_wifi_info(void) { ++notifications; }
int64_t esp_timer_get_time(void) { return 1000000; }

int main(void)
{
    app_ui_status_t status;
    app_ui_get_status(&status);
    assert(status.battery == 255 && status.focus == 0xffff && status.ev == INT32_MIN);
    assert(!status.settings && !status.failed && status.fps_tenths == 0);
    app_ui_set_wifi_info("test-ap", "test-only", false, "192.0.2.1", true);
    app_ui_set_camera_property(0xd218, 75);
    app_ui_set_camera_property(0x500a, 2);
    app_ui_set_camera_property(0x5010, (uint16_t)-700);
    app_ui_get_status(&status);
    assert(status.battery == 75 && status.focus == 2 && status.ev == -700 && status.default_password);
    app_ui_set_camera_property(0xd218, 101);
    app_ui_get_status(&status); assert(status.battery == 255);
    assert(app_ui_menu_selected() == 5);
    app_ui_menu_move(1); assert(app_ui_menu_selected() == 5); /* Hidden menu does not move. */
    assert(app_ui_toggle_settings_mode());
    const unsigned order[] = {0, 1, 2, 3, 4, 6, 9, 7, 5};
    for (unsigned i = 0; i < 9; ++i) {
        app_ui_menu_move(1); assert(app_ui_menu_selected() == order[i]);
    }
    app_ui_menu_move(-1); assert(app_ui_menu_selected() == 7);
    app_ui_extra_menu_open(true);
    assert(app_ui_extra_menu_active() && app_ui_menu_selected() == 7);
    app_ui_extra_menu_move(-1); assert(app_ui_extra_menu_exit_selected());
    app_ui_extra_menu_move(1); assert(!app_ui_extra_menu_exit_selected());
    app_ui_extra_status_t extra;
    app_ui_set_camera_property(camera_extra_codes[0], 4);
    app_ui_set_menu_item(7, true, 1, true, 2);
    assert(app_ui_get_extra_status(0, &extra));
    assert(extra.actual == 4 && extra.target == 2 && extra.status == 1 && extra.writable && extra.target_valid);
    assert(!app_ui_get_extra_status(CAMERA_EXTRA_COUNT, &extra));
    assert(!app_ui_toggle_settings_mode() && !app_ui_extra_menu_active());
    app_ui_set_atom_status(true, true); assert(notifications == 1);
    app_ui_set_atom_status(true, true); assert(notifications == 1);
    app_ui_set_atom_status(false, false); assert(notifications == 2);
    app_ui_set_sim(true);
    app_ui_set_atom_status(false,true);
    assert(!atomic_load(&ui_model_atom_connected) && atomic_load(&ui_model_controller_connected));
    app_ui_set_sim(false);
    app_ui_set_atom_status(false,true);
    assert(!atomic_load(&ui_model_controller_connected));
    unsigned protocol_before=notifications;
    app_ui_set_atom_protocol(false,1,true);
    assert(atomic_load(&ui_model_gimbal_fault) && notifications==protocol_before+1);
    app_ui_set_atom_protocol(false,1,true);
    assert(notifications==protocol_before+1);
    app_ui_set_atom_protocol(false,1,false);
    assert(!atomic_load(&ui_model_gimbal_fault) && notifications==protocol_before+2);
    app_ui_set_atom_protocol(false,1,true);
    app_ui_set_camera_info("camera","version");app_ui_set_recording_status(true,true);
    atomic_store(&ui_model_display_failed,true); /* Health survives normal clear. */
    unsigned before=notifications,generation=app_ui_connection_generation();
    ui_model_freeze_and_clear();assert(atomic_load(&ui_model_frozen));
    assert(!ui_model_connection_ssid[0] && !ui_model_connection_password[0] && !ui_model_connection_ip[0]);
    assert(!ui_model_camera_model[0] && !ui_model_camera_firmware[0]);
    app_ui_get_status(&status);assert(status.failed && !status.default_password && !status.settings && status.battery==255);
    app_ui_set_wifi_info("late","late",true,"late",true);app_ui_set_camera_info("late","late");
    app_ui_set_info_level(2);app_ui_set_sim(true);app_ui_set_atom_status(true,true);
    app_ui_set_atom_protocol(true,3,true);app_ui_set_controller_battery(10,true);
    app_ui_set_wifi_rssi(-20);app_ui_set_exposure_mode(1);app_ui_set_camera_property(0xd218,80);
    app_ui_extra_menu_open(true);app_ui_extra_menu_move(1);app_ui_menu_move(1);
    app_ui_set_menu_item(7,true,1,true,2);app_ui_set_command_status(0x500e,1);app_ui_set_recording_status(true,true);
    assert(!app_ui_toggle_settings_mode());
    assert(!ui_model_connection_ssid[0] && !ui_model_camera_model[0] && !atomic_load(&ui_model_sim_active));
    assert(!atomic_load(&ui_model_atom_connected) && !atomic_load(&ui_model_controller_connected));
    assert(!atomic_load(&ui_model_gimbal_fault));
    assert(atomic_load(&ui_model_wifi_rssi)==-127 && atomic_load(&ui_model_camera_battery)==255);
    assert(!atomic_load(&ui_model_mode_command_status) && !atomic_load(&ui_model_recording_state));
    assert(app_ui_get_extra_status(0,&extra) && extra.actual==UINT32_MAX && !extra.writable && !extra.target_valid);
    assert(notifications==before && app_ui_connection_generation()==generation+1);
    ui_model_freeze_and_clear();assert(app_ui_connection_generation()==generation+1);
    puts("UI model navigation, terminal clear/freeze and rejected late mutation passed");
    return 0;
}
