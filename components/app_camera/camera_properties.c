#include "camera_properties.h"
#include <string.h>

/* Menu order is the established UI/navigation order; backend IDs stay private.
 * No PTP/vendor code or implementation type is used by this collection layer. */
const camera_setting_t camera_menu_settings[CAMERA_MENU_COUNT] = {
    CAMERA_SETTING_SHUTTER, CAMERA_SETTING_APERTURE, CAMERA_SETTING_ISO,
    CAMERA_SETTING_EV, CAMERA_SETTING_WB, CAMERA_SETTING_FOCUS, CAMERA_SETTING_METER,
    CAMERA_SETTING_ASPECT, CAMERA_SETTING_DRIVE, CAMERA_SETTING_EFFECT,
    CAMERA_SETTING_DRO, CAMERA_SETTING_AF_AREA, CAMERA_SETTING_WL_FLASH,
    CAMERA_SETTING_WB_TEMP, CAMERA_SETTING_WB_AB, CAMERA_SETTING_WB_GM
};
static void collect(void *context, const camera_property_t *p)
{
    camera_properties_t *s = context;
    if (!s->valid) return;
    if (!p || (unsigned)p->setting >= CAMERA_SETTING_COUNT ||
        (unsigned)p->current.type > CAMERA_VALUE_I32 || s->seen[p->setting] ||
        p->choice_count > 64 || (p->choice_count && !p->choices)) { s->valid = false; return; }
    unsigned width = p->current.type <= CAMERA_VALUE_I8 ? 1 :
        p->current.type <= CAMERA_VALUE_I16 ? 2 : 4;
    if ((width == 1 && p->current.bits > UINT8_MAX) || (width == 2 && p->current.bits > UINT16_MAX)) {
        s->valid = false; return;
    }
    camera_choice_state_t choices = {.current = p->current.bits,
        .writable = p->writable, .count = p->choice_count};
    for (unsigned i = 0; i < p->choice_count; ++i) {
        if (p->choices[i].type != p->current.type ||
            (width == 1 && p->choices[i].bits > UINT8_MAX) ||
            (width == 2 && p->choices[i].bits > UINT16_MAX)) { s->valid = false; return; }
        choices.values[i] = p->choices[i].bits;
    }
    s->seen[p->setting] = true; s->values[p->setting] = p->current;
    if (p->setting == CAMERA_SETTING_MODE) s->mode = choices;
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) if (p->setting == camera_menu_settings[i]) {
        if (p->relative && (i > MENU_APERTURE || p->choice_count)) { s->valid = false; return; }
        s->menu.states[i] = choices;
        /* State-machine type identity is opaque; zero means missing. Values
         * here are stable local tags, not exported protocol datatype codes. */
        s->menu.types[i] = (uint16_t)p->current.type + 1;
        s->menu.relative[i] = p->relative;
        return;
    }
}
camera_backend_result_t camera_properties_read(camera_backend_t *backend,
    void *scratch, size_t capacity, uint32_t timeout, camera_properties_t *snapshot)
{
    if (!backend || !snapshot || !scratch || !capacity || !timeout) return CAMERA_BACKEND_INVALID;
    memset(snapshot, 0, sizeof *snapshot);
    if (backend->api_version != CAMERA_BACKEND_API_VERSION || !backend->ops ||
        !(backend->capabilities & CAMERA_BACKEND_CAP_PROPERTIES) || !backend->ops->properties)
        return CAMERA_BACKEND_UNSUPPORTED;
    snapshot->valid = true;
    camera_backend_result_t result = backend->ops->properties(backend->context, scratch,
        capacity, timeout, collect, snapshot, &snapshot->capabilities);
    if (result == CAMERA_BACKEND_OK && !snapshot->valid) result = CAMERA_BACKEND_PROTOCOL;
    if (result != CAMERA_BACKEND_OK) memset(snapshot, 0, sizeof *snapshot);
    else snapshot->menu.valid = true;
    return result;
}
bool camera_properties_apply(const camera_properties_t *snapshot,
    setting_control_t *mode, camera_menu_t *menu, uint32_t now)
{
    if (!snapshot || !mode || !menu) return false;
    camera_choice_state_t empty = {0};
    setting_control_snapshot(mode, snapshot->valid ? &snapshot->mode : &empty, now);
    camera_menu_snapshot_t missing = {0};
    camera_menu_snapshot_apply(menu, snapshot->valid ? &snapshot->menu : &missing, now);
    return snapshot->valid;
}
