#pragma once
#include <stddef.h>
#include <stdint.h>
#define CAMERA_EXTRA_COUNT 9
/* UINT32_MAX means absent; unknown enum values are displayed without truncation. */
void camera_extra_format(unsigned index, uint32_t value, char *text, size_t size);
