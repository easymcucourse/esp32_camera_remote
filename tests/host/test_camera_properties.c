#include "camera_properties.h"
#include <assert.h>
#include <string.h>
static camera_value_t mode_choices[] = {{CAMERA_VALUE_U32,1},{CAMERA_VALUE_U32,2},{CAMERA_VALUE_U32,3}};
static camera_value_t ev_choices[] = {{CAMERA_VALUE_I16,0xff00},{CAMERA_VALUE_I16,0xffff},{CAMERA_VALUE_I16,0}};
static camera_property_t properties[3];
static camera_backend_result_t result;
static unsigned count;
static bool duplicate, invalid_type;
static camera_backend_result_t read_properties(void *context, void *scratch, size_t capacity,
    uint32_t timeout, camera_property_visitor_t visit, void *visitor, camera_capabilities_t *caps)
{
    assert(context == &count && scratch && capacity == 16 && timeout == 5000);
    *caps = (camera_capabilities_t){.recording_known = true};
    for (unsigned i = 0; i < count; ++i) visit(visitor, &properties[i]);
    if (duplicate) visit(visitor, &properties[0]);
    if (invalid_type) {
        camera_property_t broken = {.setting = CAMERA_SETTING_ISO, .current = {(camera_value_type_t)99,1}};
        visit(visitor, &broken);
    }
    return result;
}
static const camera_backend_ops_t ops = {.properties = read_properties};
static camera_backend_t backend = {.api_version = CAMERA_BACKEND_API_VERSION,
    .capabilities = CAMERA_BACKEND_CAP_PROPERTIES, .ops = &ops, .context = &count};
static uint8_t scratch[16];
static camera_properties_t snapshot;
static void reset(void)
{
    properties[0] = (camera_property_t){.setting = CAMERA_SETTING_MODE,
        .current = {CAMERA_VALUE_U32,1}, .writable = true, .choices = mode_choices, .choice_count = 3};
    properties[1] = (camera_property_t){.setting = CAMERA_SETTING_EV,
        .current = {CAMERA_VALUE_I16,0xffff}, .writable = true, .choices = ev_choices, .choice_count = 3};
    properties[2] = (camera_property_t){.setting = CAMERA_SETTING_APERTURE,
        .current = {CAMERA_VALUE_U16,400}, .writable = true, .relative = true};
    result = CAMERA_BACKEND_OK; count = 3; duplicate = invalid_type = false;
}
static camera_backend_result_t read(void)
{ return camera_properties_read(&backend, scratch, sizeof scratch, 5000, &snapshot); }
int main(void)
{
    reset(); assert(read() == CAMERA_BACKEND_OK && snapshot.valid);
    assert(snapshot.menu.types[MENU_EV] == CAMERA_VALUE_I16 + 1 && snapshot.menu.relative[MENU_APERTURE]);
    assert(snapshot.seen[CAMERA_SETTING_MODE] && snapshot.values[CAMERA_SETTING_EV].bits == 0xffff);
    assert(snapshot.capabilities.recording_known && !snapshot.capabilities.recording);
    /* No borrowed callback buffer survives: mutation does not change snapshot. */
    mode_choices[1].bits = 99; assert(snapshot.mode.values[1] == 2); mode_choices[1].bits = 2;
    setting_control_t mode = {0}; camera_menu_t menu = {0};
    assert(camera_properties_apply(&snapshot, &mode, &menu, 0));
    assert(setting_control_step(&mode, 1)); uint32_t target;
    assert(setting_control_next(&mode, 0, &target) && target == 2); setting_control_response(&mode, true);
    assert(camera_properties_apply(&snapshot, &mode, &menu, 1000) && mode.awaiting);
    properties[0].current.bits = 2; assert(read() == CAMERA_BACKEND_OK);
    assert(camera_properties_apply(&snapshot, &mode, &menu, 2000));
    assert(!mode.awaiting && mode.status == SETTING_APPLIED);
    assert(camera_menu_step(&menu, MENU_EV, 1, true)); camera_menu_write_t write;
    assert(camera_menu_next(&menu, MENU_EV, 2000, &write) && write.value == 0 && write.code == 0);
    camera_menu_response(&menu, MENU_EV, true);
    assert(camera_properties_apply(&snapshot, &mode, &menu, 2100) && menu.items[MENU_EV].control.awaiting);
    properties[1].current.bits = 0; assert(read() == CAMERA_BACKEND_OK);
    camera_properties_apply(&snapshot, &mode, &menu, 2200);
    assert(camera_menu_status(&menu, MENU_EV) == SETTING_APPLIED);
    assert(camera_menu_step(&menu, MENU_APERTURE, -1, true));
    assert(camera_menu_next(&menu, MENU_APERTURE, 2300, &write) && write.relative && write.value == 255);
    camera_menu_response(&menu, MENU_APERTURE, true);
    camera_properties_apply(&snapshot, &mode, &menu, 2400); assert(menu.items[MENU_APERTURE].awaiting);
    properties[2].current.bits = 280; assert(read() == CAMERA_BACKEND_OK);
    camera_properties_apply(&snapshot, &mode, &menu, 2500);
    assert(camera_menu_status(&menu, MENU_APERTURE) == SETTING_APPLIED);
    /* Missing descriptors invalidate pending writes without keeping old targets. */
    assert(camera_menu_step(&menu, MENU_EV, -1, true)); count = 1; assert(read() == CAMERA_BACKEND_OK);
    camera_properties_apply(&snapshot, &mode, &menu, 2600);
    assert(!menu.items[MENU_EV].writable && !menu.items[MENU_EV].control.dirty);
    assert(!snapshot.seen[CAMERA_SETTING_EV]);
    reset(); duplicate = true; assert(read() == CAMERA_BACKEND_PROTOCOL && !snapshot.valid && !snapshot.mode.count);
    reset(); invalid_type = true; assert(read() == CAMERA_BACKEND_PROTOCOL && !snapshot.valid);
    reset(); properties[1].choice_count = 65; assert(read() == CAMERA_BACKEND_PROTOCOL);
    reset(); properties[1].choices = NULL; assert(read() == CAMERA_BACKEND_PROTOCOL);
    reset(); properties[1].current.bits = 65536; assert(read() == CAMERA_BACKEND_PROTOCOL);
    reset(); properties[1].relative = true; assert(read() == CAMERA_BACKEND_PROTOCOL);
    reset(); ev_choices[0].type = CAMERA_VALUE_U16; assert(read() == CAMERA_BACKEND_PROTOCOL); ev_choices[0].type = CAMERA_VALUE_I16;
    reset(); result = CAMERA_BACKEND_TIMEOUT; assert(read() == CAMERA_BACKEND_TIMEOUT && !snapshot.valid && !snapshot.mode.count);
    camera_properties_apply(&snapshot, &mode, &menu, 2700);
    assert(!mode.snapshot.writable && !menu.items[MENU_APERTURE].writable);
    backend.api_version++; assert(read() == CAMERA_BACKEND_UNSUPPORTED && !snapshot.valid);
    assert(camera_properties_read(NULL, scratch, sizeof scratch, 5000, &snapshot) == CAMERA_BACKEND_INVALID);
    return 0;
}
