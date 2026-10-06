#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const uint8_t *jpeg;
    size_t jpeg_size;
} sony_liveview_t;

/* A fully received GetObject payload. Borrowed bytes remain owned by the caller.
 * Invalid image data is a dropped frame, not a PTP/IP transport failure. */
bool sony_liveview_parse(const uint8_t *object, size_t size, sony_liveview_t *view);
