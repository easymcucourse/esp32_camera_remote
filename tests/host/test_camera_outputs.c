#include "camera_outputs.h"
#include "app_console.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static app_message_t held;
static bool reject;
static unsigned allocations, freed;
void *test_output_calloc(size_t n, size_t size) { ++allocations; return calloc(n, size); }
void test_output_free(void *pointer) { ++freed; free(pointer); }
esp_err_t app_console_send(app_message_t *message)
{
    assert(message->source == APP_ENDPOINT_CAMERA && message->generation == 8 && (message->flags & APP_MESSAGE_EVENT));
    if (reject) { app_message_release(message); return ESP_ERR_INVALID_STATE; }
    held = *message; message->lease = NULL; return ESP_OK;
}
int main(void)
{
    camera_properties_t properties = {.valid = true}; setting_control_t mode = {0}; camera_menu_t menu = {0};
    gamepad_caps_t caps = {.recording_known = true, .recording = true}; app_camera_view_t view;
    properties.seen[CAMERA_SETTING_EV] = true; properties.values[CAMERA_SETTING_EV] = (camera_value_t){CAMERA_VALUE_I16,0xffff};
    properties.seen[CAMERA_SETTING_FLASH] = true; properties.values[CAMERA_SETTING_FLASH] = (camera_value_t){CAMERA_VALUE_U16,2};
    mode.status = SETTING_PENDING; mode.dirty = true; mode.desired = 3;
    menu.items[MENU_EV].writable = true; menu.items[MENU_EV].control.dirty = true; menu.items[MENU_EV].control.desired = 0;
    assert(camera_view_build(&properties, &mode, &menu, &caps, &view));
    assert(view.properties[APP_CAMERA_PROPERTY_EV].actual == 0xffff && view.properties[APP_CAMERA_PROPERTY_EV].actual_valid);
    assert(view.properties[APP_CAMERA_PROPERTY_EV].writable && view.properties[APP_CAMERA_PROPERTY_EV].target_valid);
    assert(view.properties[APP_CAMERA_PROPERTY_MODE].status == SETTING_PENDING && view.properties[APP_CAMERA_PROPERTY_MODE].target == 3);
    assert(view.properties[APP_CAMERA_PROPERTY_FLASH].actual == 2);
    assert(view.properties[APP_CAMERA_PROPERTY_ASPECT].actual_valid && view.properties[APP_CAMERA_PROPERTY_ASPECT].actual == UINT32_MAX);
    assert(view.capabilities.recording_known && view.capabilities.recording);
    properties.valid = false;
    app_camera_view_t invalid;
    assert(camera_view_build(&properties, &mode, &menu, &caps, &invalid));
    assert(!invalid.properties[APP_CAMERA_PROPERTY_EV].writable && !invalid.properties[APP_CAMERA_PROPERTY_EV].target_valid);
    assert(!invalid.properties[APP_CAMERA_PROPERTY_MODE].writable && !invalid.properties[APP_CAMERA_PROPERTY_MODE].target_valid);
    properties.valid = true;
    assert(camera_outputs_properties(8, &view) == ESP_OK && allocations == 1 && !freed);
    size_t size; const app_camera_view_t *copy = app_message_lease_data(held.lease, &size);
    assert(copy && size == sizeof *copy && !app_message_lease_write(held.lease, NULL));
    view.properties[APP_CAMERA_PROPERTY_EV].actual = 99;
    assert(copy->properties[APP_CAMERA_PROPERTY_EV].actual == 0xffff); /* Producer storage is copied. */
    app_message_release(&held); assert(freed == 1);
    reject = true; assert(camera_outputs_properties(8, &view) == ESP_ERR_INVALID_STATE && allocations == 2 && freed == 2);
    assert(camera_outputs_properties(0, &view) == ESP_ERR_INVALID_ARG && allocations == 2);
    reject = false;
    app_camera_status_t status = {.stage = APP_CAMERA_STAGE_LIVE};
    assert(camera_outputs_state(8, &status) == ESP_OK && held.payload.camera.stage == APP_CAMERA_STAGE_LIVE && !held.lease);
    assert(camera_outputs_command(8, APP_CAMERA_CONTROL_RECORD, SETTING_PENDING) == ESP_OK && held.payload.command.index == APP_CAMERA_CONTROL_RECORD);
    assert(camera_outputs_command(8, APP_CAMERA_CONTROL_COUNT, 0) == ESP_ERR_INVALID_ARG);
    assert(camera_outputs_command(8, APP_CAMERA_CONTROL_RECORD, 99) == ESP_ERR_INVALID_ARG);
    return 0;
}
