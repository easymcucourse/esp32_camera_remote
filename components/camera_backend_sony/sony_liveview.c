#include "sony_liveview.h"

bool sony_liveview_parse(const uint8_t *object, size_t size, sony_liveview_t *view)
{
    if (!view) return false;
    *view = (sony_liveview_t){0};
    if (!object || size < 4) return false;
    size_t offset = (uint32_t)object[0] | ((uint32_t)object[1] << 8) |
                    ((uint32_t)object[2] << 16) | ((uint32_t)object[3] << 24);
    if (offset < 4 || offset > size || size - offset < 4) return false;
    const uint8_t *jpeg = object + offset;
    size_t length = size - offset;
    if (jpeg[0] != 0xff || jpeg[1] != 0xd8) return false;

    bool scan = false, had_scan = false;
    size_t pos = 2;
    while (pos < length) {
        if (scan) {
            while (pos < length && jpeg[pos] != 0xff) ++pos;
        }
        if (pos >= length || jpeg[pos++] != 0xff) return false;
        while (pos < length && jpeg[pos] == 0xff) ++pos; /* Marker fill bytes. */
        if (pos >= length) return false;
        uint8_t marker = jpeg[pos++];
        if (scan && (marker == 0 || (marker >= 0xd0 && marker <= 0xd7))) continue;
        if (marker == 0xd9) {
            if (!had_scan) return false;
            view->jpeg = jpeg;
            view->jpeg_size = pos;
            return true;
        }
        if (marker == 0x01) continue; /* TEM has no length. */
        if (marker < 0xc0 || marker == 0xd8 || (marker >= 0xd0 && marker <= 0xd7)) return false;
        if (length - pos < 2) return false;
        size_t segment = ((size_t)jpeg[pos] << 8) | jpeg[pos + 1];
        if (segment < 2 || segment > length - pos) return false;
        pos += segment; /* APP/COM payloads can contain bytes that look like EOI. */
        scan = marker == 0xda;
        had_scan |= scan;
    }
    return false;
}
