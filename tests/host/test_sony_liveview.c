#include "sony_liveview.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* APP payload contains fake SOI/EOI; entropy includes byte stuffing and restart.
 * A second scan exercises progressive/multi-scan marker transitions. */
static const uint8_t jpeg[] = {
    0xff,0xd8, 0xff,0xe1,0,8, 0xff,0xd9,0xff,0xd8,1,2,
    0xff,0xda,0,2, 1,0xff,0,0xd9,2,0xff,0xd0,3,
    0xff,0xc4,0,3,4, 0xff,0xda,0,2,5,0xff,0xff,0xd9
};
int main(void)
{
    uint8_t object[256]; sony_liveview_t view;
    for (unsigned offset = 136; offset <= 160; offset += 24) {
        memset(object, 0, sizeof(object)); object[0] = (uint8_t)offset;
        object[4] = 0xff; /* Unverified second header field is never JPEG length. */
        memcpy(object + offset, jpeg, sizeof(jpeg));
        object[offset + sizeof(jpeg)] = 0xff; object[offset + sizeof(jpeg) + 1] = 0xd9;
        assert(sony_liveview_parse(object, sizeof(object), &view));
        assert(view.jpeg == object + offset && view.jpeg_size == sizeof(jpeg));
        for (size_t n = 0; n < offset + sizeof(jpeg); ++n) {
            assert(!sony_liveview_parse(object, n, &view));
            assert(!view.jpeg && !view.jpeg_size);
        }
        /* Segment lengths must fit; markers hidden inside APP are not boundaries. */
        object[offset + 5] = 1; assert(!sony_liveview_parse(object, sizeof(object), &view));
        object[offset + 5] = 255; assert(!sony_liveview_parse(object, sizeof(object), &view));
        object[offset + 5] = 8;
        object[offset] = 0; assert(!sony_liveview_parse(object, sizeof(object), &view));
    }
    memset(object, 0xff, sizeof(object)); assert(!sony_liveview_parse(object, sizeof(object), &view));
    object[0] = 2; object[1] = object[2] = object[3] = 0;
    assert(!sony_liveview_parse(object, sizeof(object), &view));
    assert(!sony_liveview_parse(NULL, 10, &view)); assert(!sony_liveview_parse(object, 10, NULL));
    puts("Sony JPEG offsets, markers, truncation and trailing data passed");
}
