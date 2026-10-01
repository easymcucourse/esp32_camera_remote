#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
static inline uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void put32(uint8_t *p, uint32_t value)
{
    for (int i = 0; i < 4; ++i) p[i] = value >> (8 * i);
}

static inline uint16_t get16(const uint8_t *p)
{
    return (uint16_t)p[0] | (uint16_t)p[1] << 8;
}

