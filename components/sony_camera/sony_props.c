#include "sony_codes.h"
#include "ptp_codes.h"
#include "sony_props.h"
#include "ptpip_packet.h"
#include <string.h>

static bool skip_value(const uint8_t *data, size_t size, size_t *at, uint16_t type)
{
    if (*at > size) return false;
    if (type == 0xffff) {
        if (*at == size) return false;
        size_t bytes = 1 + (size_t)data[*at] * 2;
        if (bytes > size - *at) return false;
        *at += bytes;
        return true;
    }
    bool array = (type & 0x4000) != 0;
    unsigned base = type & ~0x4000u;
    if (base < 1 || base > 10) return false;
    size_t width = (size_t)1 << ((base - 1) / 2), count = 1;
    if (array) {
        if (size - *at < 4) return false;
        count = get32(data + *at);
        *at += 4;
    }
    if (count > (size - *at) / width) return false;
    *at += count * width;
    return true;
}

static bool parse_focus_caps(const uint8_t *data, size_t size, unsigned lists,
                             sony_focus_caps_t *out, sony_property_visitor_t visit, void *context)
{
    sony_focus_caps_t caps = {0};
    if (!data || size < 8 || get32(data + 4) != 0) return false;
    uint32_t count = get32(data);
    if (count > (size - 8) / 7) return false;
    size_t at = 8;
    for (uint32_t i = 0; i < count; ++i) {
        if (size - at < 6) return false;
        uint16_t code = get16(data + at), type = get16(data + at + 2);
        at += 6;
        if (!skip_value(data, size, &at, type)) return false;
        size_t current = at;
        if (!skip_value(data, size, &at, type)) return false;
        if (visit && type >= 1 && type <= 6) {
            unsigned width = 1u << ((type - 1) / 2);
            uint32_t value = width == 1 ? data[current] :
                             width == 2 ? get16(data + current) : get32(data + current);
            visit(context, code, value);
        }
        if (code == SONY_DPC_FOCUS_MODE && type == 4) {
            if (caps.focus_known) return false;
            caps.focus_known = true;
            caps.focus_mode = get16(data + current);
        }
        if (code == SONY_DPC_ZOOM_ENABLE_STATUS && type == 2) {
            if (caps.zoom_known) return false;
            caps.zoom_enabled = data[current];
            caps.zoom_known = caps.zoom_enabled <= 1;
        }
        if (at == size) return false;
        uint8_t form = data[at++];
        if (form == 1) {
            for (unsigned j = 0; j < 3; ++j)
                if (!skip_value(data, size, &at, type)) return false;
        } else if (form == 2) {
            for (unsigned list = 0; list < lists; ++list) {
                if (size - at < 2) return false;
                unsigned values = get16(data + at);
                at += 2;
                if (values > size - at) return false;
                for (unsigned j = 0; j < values; ++j)
                    if (!skip_value(data, size, &at, type)) return false;
            }
        } else if (form != 0) return false;
    }
    if (at != size) return false;
    *out = caps;
    return true;
}

bool sony_parse_focus_caps(const uint8_t *data, size_t size, sony_focus_caps_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    /* Accept a layout only if every declared entry consumes the whole dataset. */
    return parse_focus_caps(data, size, 2, out, NULL, NULL) ||
           parse_focus_caps(data, size, 1, out, NULL, NULL);
}

bool sony_parse_scalar_properties(const uint8_t *data, size_t size,
                                  sony_property_visitor_t visit, void *context)
{
    sony_focus_caps_t caps;
    unsigned lists = 2;
    if (!parse_focus_caps(data, size, lists, &caps, NULL, NULL)) {
        lists = 1;
        if (!parse_focus_caps(data, size, lists, &caps, NULL, NULL)) return false;
    }
    /* Publish only after the entire dataset passes validation. */
    return parse_focus_caps(data, size, lists, &caps, visit, context);
}

static bool sony_property_value(const uint8_t *data, size_t size, uint16_t code,
                                uint16_t type, uint32_t *value)
{
    // Sony 0x9209 entries are not standard PTP DevicePropDesc records:
    // code:u16, type:u16, getset:u8, enabled:u8, default, current, form...
    // The capture starts with entry_count:u32 and reserved:u32.
    unsigned width = type <= 2 ? 1 : type <= 4 ? 2 : type <= 6 ? 4 : 0;
    if (!width || size < 8 || get32(data + 4) != 0) return false;
    for (size_t i = 8; i + 6 + width * 2 < size; ++i) {
        if (get16(data + i) == code && get16(data + i + 2) == type &&
            (data[i + 4] <= 1 || (data[i + 4] & 0x80)) && data[i + 5] <= 2) {
            const uint8_t *current = data + i + 6 + width;
            *value = width == 1 ? current[0] :
                     width == 2 ? get16(current) : get32(current);
            return true;
        }
    }
    return false;
}

void sony_parse_properties(const uint8_t *data, size_t size, sony_mode_state_t *mode,
                           sony_property_visitor_t visit, void *context)
{
    mode->count = 0;
    mode->writable = false;
    // Sony UINT32 ExposureProgram: header(6), default(4), current(4), enum form.
    if (size >= 8 && get32(data + 4) == 0) {
        for (size_t i = 8; i + 17 <= size; ++i) {
            if (get16(data + i) != SONY_DPC_EXPOSURE_PROGRAM || get16(data + i + 2) != 6 ||
                data[i + 5] > 2 || data[i + 14] != 2) continue;
            unsigned count = get16(data + i + 15);
            if (!count || count > 64 || count > (size - i - 17) / 4) continue;
            mode->current = get32(data + i + 10);
            mode->count = count;
            mode->writable = data[i + 4] == 1 && data[i + 5] == 1;
            for (unsigned j = 0; j < count; ++j) mode->values[j] = get32(data + i + 17 + j * 4);
            break;
        }
    }
    static const struct { uint16_t code, type; } properties[] = {
        {SONY_DPC_WHITE_BALANCE, 4}, {SONY_DPC_F_NUMBER, 4}, {SONY_DPC_FOCUS_MODE, 4}, {SONY_DPC_METERING, 4},
        {SONY_DPC_FLASH, 4}, {SONY_DPC_EXPOSURE_BIAS, 3}, {SONY_DPC_SHUTTER_SPEED, 6}, {SONY_DPC_ISO, 6},
    };
    uint32_t value;
    if (sony_property_value(data, size, SONY_DPC_EXPOSURE_PROGRAM, 6, &value)) {
        mode->current = value;
        visit(context, SONY_DPC_EXPOSURE_PROGRAM, value);
    }
    for (unsigned i = 0; i < sizeof(properties) / sizeof(properties[0]); ++i) {
        if (sony_property_value(data, size, properties[i].code, properties[i].type, &value))
            visit(context, properties[i].code, value);
    }
}

