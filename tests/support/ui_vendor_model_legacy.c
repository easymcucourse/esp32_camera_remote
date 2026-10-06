/* Retained original model setters for historical assertions only. */
#include "ui_model.h"
#include "legacy/ui_camera_vendor_codes.h"
#include "esp_timer.h"

void app_ui_set_camera_property(uint16_t code, uint32_t value)
{
    if (atomic_load(&ui_model_frozen)) return;
    switch (code) {
    case 0x5005: atomic_store(&ui_model_prop_wb, value); break;
    case 0x5007: atomic_store(&ui_model_prop_aperture, value); break;
    case 0x500a: atomic_store(&ui_model_prop_focus, value); break;
    case 0x500b: atomic_store(&ui_model_prop_meter, value); break;
    case 0x500c: atomic_store(&ui_model_prop_flash, value); break;
    case 0x5010: atomic_store(&ui_model_prop_ev, (int16_t)value); break;
    case 0xd20d: atomic_store(&ui_model_prop_shutter, value); break;
    case 0xd21e: atomic_store(&ui_model_prop_iso, value); break;
    case 0xd218: atomic_store(&ui_model_camera_battery, value <= 100 ? value : 255); break;
    default:
        for (unsigned i = 0; i < CAMERA_EXTRA_COUNT; ++i)
            if (code == camera_extra_codes[i]) atomic_store(&ui_model_prop_extra[i], value);
        break;
    }
}

void app_ui_set_command_status(uint16_t code, unsigned status)
{
    if (atomic_load(&ui_model_frozen)) return;
    if (status > 5) return;
    atomic_uint *value = NULL, *changed = NULL;
    if (code == 0x500e) { value = &ui_model_mode_command_status; changed = &ui_model_mode_status_ms; }
    if (code == 0x500a) { value = &ui_model_focus_command_status; changed = &ui_model_focus_status_ms; }
    if (code == 0xd2c1 || code == 0xd2c2 || code == 0xd2c8 || code == 0xd2dd) {
        value = &ui_model_action_command_status; changed = &ui_model_action_status_ms;
    }
    if (value && atomic_exchange(value, status) != status)
        atomic_store(changed, (unsigned)(esp_timer_get_time() / 1000));
}
