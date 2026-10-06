#include "ui_camera_messages.h"
#include "ui_model.h"
#include "esp_timer.h"
#include <string.h>
static uint32_t generation;
bool ui_camera_generation_accept(uint32_t next)
{
    if (!next || (generation && (int32_t)(next - generation) < 0)) return false;
    generation = next; return true;
}
static void status(atomic_uint *value, atomic_uint *changed, unsigned next)
{
    if (atomic_exchange(value, next) != next)
        atomic_store(changed, (unsigned)(esp_timer_get_time() / 1000));
}
static void actual(app_camera_property_t property, uint32_t value)
{
    switch (property) {
    case APP_CAMERA_PROPERTY_MODE: atomic_store(&ui_model_exposure_mode, value); break;
    case APP_CAMERA_PROPERTY_SHUTTER: atomic_store(&ui_model_prop_shutter, value); break;
    case APP_CAMERA_PROPERTY_APERTURE: atomic_store(&ui_model_prop_aperture, value); break;
    case APP_CAMERA_PROPERTY_ISO: atomic_store(&ui_model_prop_iso, value); break;
    case APP_CAMERA_PROPERTY_EV: atomic_store(&ui_model_prop_ev, (int16_t)value); break;
    case APP_CAMERA_PROPERTY_WB: atomic_store(&ui_model_prop_wb, value); break;
    case APP_CAMERA_PROPERTY_FOCUS: atomic_store(&ui_model_prop_focus, value); break;
    case APP_CAMERA_PROPERTY_METER: atomic_store(&ui_model_prop_meter, value); break;
    case APP_CAMERA_PROPERTY_FLASH: atomic_store(&ui_model_prop_flash, value); break;
    case APP_CAMERA_PROPERTY_BATTERY: atomic_store(&ui_model_camera_battery, value <= 100 ? value : 255); break;
    default:
        if (property >= APP_CAMERA_PROPERTY_ASPECT && property <= APP_CAMERA_PROPERTY_WB_GM)
            atomic_store(&ui_model_prop_extra[property - APP_CAMERA_PROPERTY_ASPECT], value);
        break;
    }
}
static void caps(const gamepad_caps_t *capabilities)
{ app_ui_set_recording_status(capabilities->recording_known, capabilities->recording); }
esp_err_t ui_camera_message_apply(const app_message_t *message)
{
    if (!message || message->source != APP_ENDPOINT_CAMERA || !message->generation)
        return ESP_ERR_INVALID_ARG;
    if (message->type == APP_MESSAGE_CAMERA_PROPERTIES) {
        size_t size = 0;
        const app_camera_view_t *view = app_message_lease_data(message->lease, &size);
        if (!view || size != sizeof *view || app_message_lease_write(message->lease, NULL)) return ESP_ERR_INVALID_ARG;
        for (unsigned i = 0; i < APP_CAMERA_PROPERTY_COUNT; ++i)
            if (view->properties[i].property != (app_camera_property_t)i || view->properties[i].status > 5)
                return ESP_ERR_INVALID_ARG;
        if (!ui_camera_generation_accept(message->generation)) return ESP_ERR_INVALID_STATE;
        for (unsigned i = 0; i < APP_CAMERA_PROPERTY_COUNT; ++i) {
            const app_camera_property_state_t *p = &view->properties[i];
            if (p->actual_valid) actual(p->property, p->actual);
            if (i >= APP_CAMERA_PROPERTY_SHUTTER && i <= APP_CAMERA_PROPERTY_METER)
                app_ui_set_menu_item(i - APP_CAMERA_PROPERTY_SHUTTER, p->writable, p->status, p->target_valid, p->target);
            if (i >= APP_CAMERA_PROPERTY_ASPECT && i <= APP_CAMERA_PROPERTY_WB_GM)
                app_ui_set_menu_item(7 + i - APP_CAMERA_PROPERTY_ASPECT, p->writable, p->status, p->target_valid, p->target);
        }
        status(&ui_model_mode_command_status, &ui_model_mode_status_ms, view->properties[APP_CAMERA_PROPERTY_MODE].status);
        status(&ui_model_focus_command_status, &ui_model_focus_status_ms, view->properties[APP_CAMERA_PROPERTY_FOCUS].status);
        caps(&view->capabilities); return ESP_OK;
    }
    if (message->type == APP_MESSAGE_CAMERA_STATE) {
        const app_camera_status_t *state = &message->payload.camera;
        if ((unsigned)state->stage > APP_CAMERA_STAGE_FAILED ||
            !memchr(state->model, 0, sizeof state->model) || !memchr(state->firmware, 0, sizeof state->firmware) ||
            !memchr(state->phase, 0, sizeof state->phase)) return ESP_ERR_INVALID_ARG;
        if (!ui_camera_generation_accept(message->generation)) return ESP_ERR_INVALID_STATE;
        app_ui_set_camera_info(state->model, state->firmware);
        if (state->stage != APP_CAMERA_STAGE_LIVE && state->stage != APP_CAMERA_STAGE_STOPPED)
            return app_ui_show_connection(state->phase);
        return ESP_OK;
    }
    if (message->type == APP_MESSAGE_CAMERA_CAPABILITIES) {
        if (!ui_camera_generation_accept(message->generation)) return ESP_ERR_INVALID_STATE;
        caps(&message->payload.capabilities); return ESP_OK;
    }
    if (message->type == APP_MESSAGE_CAMERA_COMMAND_STATUS) {
        if (message->payload.command.index >= APP_CAMERA_CONTROL_COUNT || message->payload.command.value > 5)
            return ESP_ERR_INVALID_ARG;
        if (!ui_camera_generation_accept(message->generation)) return ESP_ERR_INVALID_STATE;
        status(&ui_model_action_command_status, &ui_model_action_status_ms, message->payload.command.value);
        return ESP_OK;
    }
    return ESP_ERR_NOT_SUPPORTED;
}
