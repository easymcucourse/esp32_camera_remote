#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Shrink a decoded image in the same framebuffer, retaining its LCD stride.
 * Top-down/left-to-right writes stay behind the sampled source pixels. */
static inline bool image_shrink(uint16_t *pixels, size_t capacity, size_t width,
                                size_t height, size_t stride, size_t out_width,
                                size_t out_height)
{
    if (!pixels || !width || !height || !out_width || !out_height ||
        width > stride || out_width > width || out_height > height ||
        height > capacity / stride || capacity > SIZE_MAX / sizeof(*pixels) ||
        width > SIZE_MAX / out_width || height > SIZE_MAX / out_height) return false;
    for (size_t y=0; y<out_height; ++y) {
        size_t source_row=(y*height/out_height)*stride;
        for (size_t x=0; x<out_width; ++x)
            pixels[y*stride+x]=pixels[source_row+x*width/out_width];
    }
    return true;
}

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
