#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    bool connected;
    /* bits 0..17: Share,L3,R3,Options,up,right,down,left,L2,R2,L1,R1,
       triangle,circle,cross,square,PS,touchpad click */
    uint32_t buttons;
    int8_t lx, ly, rx, ry;
    uint8_t l2, r2;
    uint8_t battery; /* raw capacity 0..10; 255 = unavailable in short report */
} ds4_state_t;

bool ds4_parse_report(const uint8_t *data, size_t length, ds4_state_t *out);
