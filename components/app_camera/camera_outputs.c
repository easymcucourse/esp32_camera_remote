#include "camera_outputs.h"
#include "app_console.h"
#include <stdlib.h>
#include <string.h>
const app_camera_property_t camera_menu_properties[CAMERA_MENU_COUNT] = {
    APP_CAMERA_PROPERTY_SHUTTER, APP_CAMERA_PROPERTY_APERTURE, APP_CAMERA_PROPERTY_ISO,
    APP_CAMERA_PROPERTY_EV, APP_CAMERA_PROPERTY_WB, APP_CAMERA_PROPERTY_FOCUS, APP_CAMERA_PROPERTY_METER,
    APP_CAMERA_PROPERTY_ASPECT, APP_CAMERA_PROPERTY_DRIVE, APP_CAMERA_PROPERTY_EFFECT, APP_CAMERA_PROPERTY_DRO,
    APP_CAMERA_PROPERTY_AF_AREA, APP_CAMERA_PROPERTY_WL_FLASH, APP_CAMERA_PROPERTY_WB_TEMP,
    APP_CAMERA_PROPERTY_WB_AB, APP_CAMERA_PROPERTY_WB_GM
};
static const app_camera_property_t properties_map[CAMERA_SETTING_COUNT] = {
    APP_CAMERA_PROPERTY_MODE, APP_CAMERA_PROPERTY_SHUTTER, APP_CAMERA_PROPERTY_APERTURE,
    APP_CAMERA_PROPERTY_ISO, APP_CAMERA_PROPERTY_EV, APP_CAMERA_PROPERTY_WB, APP_CAMERA_PROPERTY_FOCUS,
    APP_CAMERA_PROPERTY_METER, APP_CAMERA_PROPERTY_ASPECT, APP_CAMERA_PROPERTY_DRIVE,
    APP_CAMERA_PROPERTY_EFFECT, APP_CAMERA_PROPERTY_DRO, APP_CAMERA_PROPERTY_AF_AREA,
    APP_CAMERA_PROPERTY_WL_FLASH, APP_CAMERA_PROPERTY_WB_TEMP, APP_CAMERA_PROPERTY_WB_AB,
    APP_CAMERA_PROPERTY_WB_GM, APP_CAMERA_PROPERTY_BATTERY, APP_CAMERA_PROPERTY_COUNT,
    APP_CAMERA_PROPERTY_COUNT, APP_CAMERA_PROPERTY_FLASH
};
bool camera_view_build(const camera_properties_t *properties, const setting_control_t *mode,
    const camera_menu_t *menu, const gamepad_caps_t *caps, app_camera_view_t *view)
{
    if (!properties || !mode || !menu || !caps || !view) return false;
    *view = (app_camera_view_t){.capabilities = *caps};
    for (unsigned i = 0; i < APP_CAMERA_PROPERTY_COUNT; ++i) {
        view->properties[i].property = i;
        if (i >= APP_CAMERA_PROPERTY_ASPECT && i <= APP_CAMERA_PROPERTY_WB_GM)
            view->properties[i].actual = UINT32_MAX, view->properties[i].actual_valid = true;
    }
    if (properties->valid) for (unsigned i = 0; i < CAMERA_SETTING_COUNT; ++i) {
        unsigned property = properties_map[i];
        if (property >= APP_CAMERA_PROPERTY_COUNT || !properties->seen[i]) continue;
        view->properties[property].actual = properties->values[i].bits;
        view->properties[property].actual_valid = true;
    }
    if (!properties->valid) return true;
    app_camera_property_state_t *m = &view->properties[APP_CAMERA_PROPERTY_MODE];
    m->status = mode->status; m->writable = mode->snapshot.writable;
    m->target = mode->desired; m->target_valid = mode->dirty || mode->awaiting;
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        app_camera_property_state_t *p = &view->properties[camera_menu_properties[i]];
        p->writable = menu->items[i].writable; p->status = camera_menu_status(menu, i);
        p->target_valid = camera_menu_target(menu, i, &p->target);
    }
    return true;
}
static app_message_t event(app_message_type_t type, uint32_t generation)
{
    return (app_message_t){.source = APP_ENDPOINT_CAMERA, .type = type,
        .flags = APP_MESSAGE_EVENT, .generation = generation};
}
static void returned(void *context) { free(context); }
esp_err_t camera_outputs_properties(uint32_t generation, const app_camera_view_t *view)
{
    if (!generation || !view) return ESP_ERR_INVALID_ARG;
    app_camera_view_t *copy = calloc(1, sizeof *copy);
    if (!copy) return ESP_ERR_NO_MEM;
    *copy = *view;
    app_message_t message = event(APP_MESSAGE_CAMERA_PROPERTIES, generation);
    message.flags |= APP_MESSAGE_BULK;
    esp_err_t error = app_message_lease_create(copy, sizeof *copy, false, returned, copy, &message.lease);
    if (error != ESP_OK) { free(copy); return error; }
    /* send consumes on both success and failure; only last return frees copy. */
    return app_console_send(&message);
}
esp_err_t camera_outputs_state(uint32_t generation, const app_camera_status_t *status)
{
    if (!generation || !status) return ESP_ERR_INVALID_ARG;
    app_message_t message = event(APP_MESSAGE_CAMERA_STATE, generation);
    message.payload.camera = *status; return app_console_send(&message);
}
esp_err_t camera_outputs_command(uint32_t generation, app_camera_control_t control, unsigned status)
{
    if (!generation || (unsigned)control >= APP_CAMERA_CONTROL_COUNT || status > 5) return ESP_ERR_INVALID_ARG;
    app_message_t message = event(APP_MESSAGE_CAMERA_COMMAND_STATUS, generation);
    message.payload.command.index = control; message.payload.command.value = status;
    return app_console_send(&message);
}
