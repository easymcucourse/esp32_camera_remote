#include "ui_camera_messages.h"
#include "ui_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned screens, returned;
void app_ui_refresh_wifi_info(void) {}
int64_t esp_timer_get_time(void) { return 1000000; }
esp_err_t app_ui_show_connection(const char *phase) { assert(phase && *phase); ++screens; return ESP_OK; }
static void release(void *context) { assert(context == &returned); ++returned; }
static app_camera_view_t view;
static app_message_t message;
static void prepare(uint32_t generation, bool writable)
{
    view = (app_camera_view_t){0};
    for (unsigned i = 0; i < APP_CAMERA_PROPERTY_COUNT; ++i) view.properties[i].property = i;
    message = (app_message_t){.type = APP_MESSAGE_CAMERA_PROPERTIES,
        .source = APP_ENDPOINT_CAMERA, .flags = APP_MESSAGE_EVENT | APP_MESSAGE_BULK, .generation = generation};
    assert(app_message_lease_create(&view, sizeof view, writable, release, &returned, &message.lease) == ESP_OK);
}
int main(void)
{
    prepare(5, false);
    view.properties[APP_CAMERA_PROPERTY_EV].actual = 0xffff;
    view.properties[APP_CAMERA_PROPERTY_EV].actual_valid = true;
    view.properties[APP_CAMERA_PROPERTY_EV].writable = true;
    view.properties[APP_CAMERA_PROPERTY_EV].target_valid = true;
    view.properties[APP_CAMERA_PROPERTY_EV].target = 0;
    view.properties[APP_CAMERA_PROPERTY_EV].status = 1;
    view.properties[APP_CAMERA_PROPERTY_BATTERY].actual = 75;
    view.properties[APP_CAMERA_PROPERTY_BATTERY].actual_valid = true;
    view.properties[APP_CAMERA_PROPERTY_FOCUS].actual = 1;
    view.properties[APP_CAMERA_PROPERTY_FOCUS].actual_valid = true;
    view.properties[APP_CAMERA_PROPERTY_MODE].actual = 2;
    view.properties[APP_CAMERA_PROPERTY_MODE].actual_valid = true;
    view.properties[APP_CAMERA_PROPERTY_MODE].status = 1;
    view.properties[APP_CAMERA_PROPERTY_ASPECT].actual = 4;
    view.properties[APP_CAMERA_PROPERTY_ASPECT].actual_valid = true;
    view.capabilities.recording_known = view.capabilities.recording = true;
    assert(ui_camera_message_apply(&message) == ESP_OK && !returned);
    app_ui_status_t status; app_ui_get_status(&status);
    assert(status.ev == -1 && status.battery == 75 && status.focus == 1);
    assert(atomic_load(&ui_model_exposure_mode) == 2 && atomic_load(&ui_model_recording_state) == 2);
    assert(ui_model_menu_view[3].writable && ui_model_menu_view[3].target_valid && ui_model_menu_view[3].status == 1);
    assert(atomic_load(&ui_model_prop_extra[0]) == 4 && atomic_load(&ui_model_mode_command_status) == 1);
    app_message_release(&message); assert(returned == 1);
    prepare(6, false); view.properties[APP_CAMERA_PROPERTY_BATTERY].property = APP_CAMERA_PROPERTY_MODE;
    view.properties[APP_CAMERA_PROPERTY_EV].actual_valid = true; view.properties[APP_CAMERA_PROPERTY_EV].actual = 99;
    assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_ARG);
    app_ui_get_status(&status); assert(status.ev == -1); app_message_release(&message);
    prepare(4, false); assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_STATE); app_message_release(&message);
    prepare(6, true); assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_ARG); app_message_release(&message);
    prepare(6, false); message.source = APP_ENDPOINT_WIFI;
    assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_ARG); app_message_release(&message);
    prepare(6, false); view.properties[APP_CAMERA_PROPERTY_BATTERY].actual = 101;
    view.properties[APP_CAMERA_PROPERTY_BATTERY].actual_valid = true;
    view.properties[APP_CAMERA_PROPERTY_ASPECT].actual = UINT32_MAX;
    view.properties[APP_CAMERA_PROPERTY_ASPECT].actual_valid = true;
    assert(ui_camera_message_apply(&message) == ESP_OK); app_message_release(&message);
    app_ui_get_status(&status); assert(status.battery == 255 && status.ev == -1);
    assert(!ui_model_menu_view[3].writable && !ui_model_menu_view[3].target_valid);
    assert(atomic_load(&ui_model_prop_extra[0]) == UINT32_MAX);
    assert(atomic_load(&ui_model_recording_state) == 2); /* Unknown preserves last recorded state. */
    message = (app_message_t){.type = APP_MESSAGE_CAMERA_CAPABILITIES, .source = APP_ENDPOINT_CAMERA, .generation = 6};
    message.payload.capabilities.recording_known = true;
    assert(ui_camera_message_apply(&message) == ESP_OK && atomic_load(&ui_model_recording_state) == 1);
    message = (app_message_t){.type = APP_MESSAGE_CAMERA_STATE, .source = APP_ENDPOINT_CAMERA, .generation = 7};
    message.payload.camera.stage = APP_CAMERA_STAGE_CONNECTING;
    snprintf(message.payload.camera.phase, sizeof message.payload.camera.phase, "Connecting");
    snprintf(message.payload.camera.model, sizeof message.payload.camera.model, "fake model");
    assert(ui_camera_message_apply(&message) == ESP_OK && screens == 1 && !strcmp(ui_model_camera_model, "fake model"));
    message.payload.camera.stage = APP_CAMERA_STAGE_LIVE; assert(ui_camera_message_apply(&message) == ESP_OK && screens == 1);
    message.payload.camera.stage = APP_CAMERA_STAGE_STOPPED; assert(ui_camera_message_apply(&message) == ESP_OK && screens == 1);
    memset(message.payload.camera.phase, 'x', sizeof message.payload.camera.phase);
    message.generation = 100; assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_ARG);
    message = (app_message_t){.type = APP_MESSAGE_CAMERA_COMMAND_STATUS, .source = APP_ENDPOINT_CAMERA, .generation = 7};
    message.payload.command.index = APP_CAMERA_CONTROL_RECORD; message.payload.command.value = 5;
    assert(ui_camera_message_apply(&message) == ESP_OK && atomic_load(&ui_model_action_command_status) == 5);
    message.payload.command.value = 99; assert(ui_camera_message_apply(&message) == ESP_ERR_INVALID_ARG);
    assert(returned == 6);
    return 0;
}
