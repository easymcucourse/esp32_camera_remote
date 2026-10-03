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

static uint32_t scalar_value(const uint8_t *data, uint16_t type)
{
    unsigned width = 1u << ((type - 1) / 2);
    return width == 1 ? data[0] : width == 2 ? get16(data) : get32(data);
}

static bool walk(const uint8_t *data, size_t size, unsigned lists,
                 sony_descriptor_visitor_t visit, void *context)
{
    if (!data || size < 8 || get32(data + 4) != 0) return false;
    uint32_t count = get32(data);
    if (count > (size - 8) / 7) return false;
    size_t at = 8;
    bool focus_seen = false, zoom_seen = false, mode_seen = false, record_seen = false;
    for (uint32_t i = 0; i < count; ++i) {
        if (size - at < 6) return false;
        sony_property_desc_t d = {
            .code = get16(data + at), .type = get16(data + at + 2),
            .getset = data[at + 4], .enabled = data[at + 5],
        };
        /* Duplicate control descriptors cannot authorize a write. */
        bool *seen = d.code == SONY_DPC_FOCUS_MODE ? &focus_seen :
                     d.code == SONY_DPC_ZOOM_ENABLE_STATUS ? &zoom_seen :
                     d.code == SONY_DPC_EXPOSURE_PROGRAM ? &mode_seen :
                     d.code == SONY_DPC_MOVIE_RECORDING_STATE ? &record_seen : NULL;
        if (seen) { if (*seen) return false; *seen = true; }
        at += 6;
        if (!skip_value(data, size, &at, d.type)) return false;
        size_t current = at;
        if (!skip_value(data, size, &at, d.type)) return false;
        d.current = data + current;
        d.current_size = at - current;
        d.scalar = d.type >= 1 && d.type <= 6;
        if (d.scalar) d.value = scalar_value(d.current, d.type);
        d.writable = d.getset == 1 && d.enabled == 1;
        if (at == size) return false;
        d.form = data[at++];
        if (d.form == 1) {
            for (unsigned j = 0; j < 3; ++j)
                if (!skip_value(data, size, &at, d.type)) return false;
        } else if (d.form == 2) {
            for (unsigned list = 0; list < lists; ++list) {
                if (size - at < 2) return false;
                uint16_t choices = get16(data + at);
                at += 2;
                if (list == 0) { d.choice_count = choices; d.choices = data + at; }
                else { d.second_choice_count = choices; d.second_choices = data + at; }
                if (choices > size - at) return false;
                for (unsigned j = 0; j < choices; ++j)
                    if (!skip_value(data, size, &at, d.type)) return false;
            }
        } else if (d.form != 0) return false;
        if (visit) visit(context, &d);
    }
    return at == size;
}

bool sony_parse_descriptors(const uint8_t *data, size_t size,
                            sony_descriptor_visitor_t visit, void *context)
{
    unsigned lists = 2;
    if (!walk(data, size, lists, NULL, NULL)) {
        lists = 1;
        if (!walk(data, size, lists, NULL, NULL)) return false;
    }
    return !visit || walk(data, size, lists, visit, context);
}

bool sony_descriptor_choice(const sony_property_desc_t *d, unsigned index, uint32_t *value)
{
    if (!d || !value || !d->scalar || d->form != 2 || !d->choices || index >= d->choice_count)
        return false;
    unsigned width = 1u << ((d->type - 1) / 2);
    *value = scalar_value(d->choices + index * width, d->type);
    return true;
}

static void collect_caps(void *context, const sony_property_desc_t *d)
{
    sony_focus_caps_t *caps = context;
    if (d->code == SONY_DPC_FOCUS_MODE && d->type == 4) {
        caps->focus_known = true;
        caps->focus_mode = (uint16_t)d->value;
    }
    if (d->code == SONY_DPC_ZOOM_ENABLE_STATUS && d->type == 2) {
        caps->zoom_known = d->value <= 1;
        caps->zoom_enabled = (uint8_t)d->value;
    }
    if (d->code == SONY_DPC_MOVIE_RECORDING_STATE && (d->type == 2 || d->type == 4) && d->value <= 1) {
        caps->recording_known = true;
        caps->recording = d->value == 1;
    }
}

bool sony_parse_focus_caps(const uint8_t *data, size_t size, sony_focus_caps_t *out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    return sony_parse_descriptors(data, size, collect_caps, out);
}

typedef struct { sony_property_visitor_t visit; void *context; } scalar_visitor_t;
static void visit_scalar(void *context, const sony_property_desc_t *d)
{
    scalar_visitor_t *v = context;
    if (d->scalar && v->visit) v->visit(v->context, d->code, d->value);
}

bool sony_parse_scalar_properties(const uint8_t *data, size_t size,
                                  sony_property_visitor_t visit, void *context)
{
    scalar_visitor_t visitor = {visit, context};
    return sony_parse_descriptors(data, size, visit_scalar, &visitor);
}

typedef struct { sony_mode_state_t *mode; scalar_visitor_t scalar; } mode_visitor_t;
static void collect_mode(void *context, const sony_property_desc_t *d)
{
    mode_visitor_t *v = context;
    if (d->code == SONY_DPC_EXPOSURE_PROGRAM && d->type == 6) {
        sony_mode_state_t *m = v->mode;
        m->current = d->value;
        if (d->form == 2 && d->choice_count <= 64) {
            m->count = d->choice_count;
            m->writable = d->writable;
            for (unsigned j = 0; j < m->count; ++j)
                sony_descriptor_choice(d, j, &m->values[j]);
        }
    }
    visit_scalar(&v->scalar, d);
}

void sony_parse_properties(const uint8_t *data, size_t size, sony_mode_state_t *mode,
                           sony_property_visitor_t visit, void *context)
{
    if (!mode) return;
    memset(mode, 0, sizeof(*mode));
    mode_visitor_t visitor = {mode, {visit, context}};
    sony_parse_descriptors(data, size, collect_mode, &visitor);
}
