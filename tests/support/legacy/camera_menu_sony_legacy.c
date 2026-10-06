#include "camera_menu_sony_legacy.h"
#include "sony_props.h"
#include "sony_codes.h"
#include <string.h>

/* Temporary dataset adapter for old production calls/unchanged protocol fixtures.
 * The state-machine kernel accepts copied generic choices and does no parsing. */
const uint16_t camera_menu_codes[CAMERA_MENU_COUNT] = {
    SONY_DPC_SHUTTER_SPEED, SONY_DPC_F_NUMBER, SONY_DPC_ISO, SONY_DPC_EXPOSURE_BIAS,
    SONY_DPC_WHITE_BALANCE, SONY_DPC_FOCUS_MODE, SONY_DPC_METERING,
    0xd211, 0x5013, 0xd21b, 0xd201, 0xd22c, 0xd262, 0xd20f, 0xd21c, 0xd210
};
/* Primary properties have fixed types; extras use their validated descriptor type. */
static const uint16_t types[MENU_ASPECT] = {6, 4, 6, 3, 4, 4, 4};
typedef struct {
    sony_mode_state_t states[CAMERA_MENU_COUNT];
    uint16_t types[CAMERA_MENU_COUNT];
    bool seen[CAMERA_MENU_COUNT], duplicate;
} legacy_snapshot_t;
static int32_t ev_value(uint32_t wire)
{
    return wire <= INT16_MAX ? (int32_t)wire : (int32_t)wire - 65536;
}
static void order_ev_choices(sony_mode_state_t *state)
{
    /* Right increases signed EV regardless of the camera's enum order.
     * Keep original 16-bit wire patterns for absolute writes/readback. */
    for (unsigned i = 1; i < state->count; ++i) {
        uint32_t value = state->values[i];
        unsigned j = i;
        while (j && ev_value(state->values[j - 1]) > ev_value(value)) {
            state->values[j] = state->values[j - 1];
            --j;
        }
        state->values[j] = value;
    }
}
static void collect(void *context, const sony_property_desc_t *d)
{
    legacy_snapshot_t *snapshot = context;
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        if (d->code != camera_menu_codes[i]) continue;
        if (snapshot->seen[i]) { snapshot->duplicate = true; return; }
        snapshot->seen[i] = true;
        if (!d->scalar || d->type < 1 || d->type > 6 || (i < MENU_ASPECT && d->type != types[i])) return;
        snapshot->types[i] = d->type;
        sony_mode_state_t *s = &snapshot->states[i];
        s->current = d->value;
        s->writable = d->writable;
        if (d->form == 2 && d->choice_count <= 64) {
            s->count = d->choice_count;
            for (unsigned j = 0; j < s->count; ++j) sony_descriptor_choice(d, j, &s->values[j]);
            if (i == MENU_EV) order_ev_choices(s);
        }
        if (d->form == 2 && d->choice_count > 64) s->writable = false;
        /* No guessed shutter/aperture tables: relative commands can operate
         * only with a real, enabled current value when no enum is supplied. */
        if (i == MENU_SHUTTER && (d->value == UINT32_MAX || (d->value && !(d->value & 0xffff)))) s->writable = false;
        if (i == MENU_APERTURE && (d->value >= 0xfffd || !d->value)) s->writable = false;
        return;
    }
}
bool camera_menu_snapshot(camera_menu_t *menu, const uint8_t *data, size_t size, uint32_t now)
{
    legacy_snapshot_t snapshot = {0};
    bool valid = sony_parse_descriptors(data, size, collect, &snapshot) && !snapshot.duplicate;
    if (!valid) memset(&snapshot, 0, sizeof(snapshot));
    camera_menu_snapshot_t converted = {.valid = valid};
    for (unsigned i = 0; i < CAMERA_MENU_COUNT; ++i) {
        converted.states[i] = snapshot.states[i];
        converted.types[i] = snapshot.types[i];
        converted.codes[i] = camera_menu_codes[i];
        converted.relative[i] = i <= MENU_APERTURE && !snapshot.states[i].count;
    }
    return camera_menu_snapshot_apply(menu, &converted, now);
}
