#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Expand tightly packed RGB565 rows in place, preserving all image pixels.
 * Bottom-up order protects later source rows. Only the few rows whose source
 * and destination overlap need memmove; disjoint rows use optimized memcpy.
 * Padding is intentionally untouched; the caller owns its clear/draw policy. */
static inline bool image_stride_expand(uint16_t *pixels, size_t capacity,
                                       size_t width, size_t height, size_t stride)
{
    if (!pixels || !width || !height || capacity > SIZE_MAX / sizeof(*pixels) || stride < width ||
        stride > SIZE_MAX / sizeof(*pixels) || height > capacity / stride) return false;
    if (width == stride) return true;
    for (size_t row = height; --row;) {
        uint16_t *source = pixels + row * width;
        uint16_t *destination = pixels + row * stride;
        if (row * (stride-width) >= width)
            memcpy(destination, source, width * sizeof(*pixels));
        else memmove(destination, source, width * sizeof(*pixels));
    }
    return true;
}
