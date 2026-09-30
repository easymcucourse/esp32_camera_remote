#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
static inline void *heap_caps_malloc(size_t bytes, unsigned caps) { return malloc(bytes); }
static inline void *heap_caps_malloc_prefer(size_t bytes, size_t count, ...) { return malloc(bytes); }
static inline void *heap_caps_calloc(size_t count, size_t bytes, unsigned caps) { return calloc(count, bytes); }
static inline void heap_caps_free(void *block) { free(block); }
