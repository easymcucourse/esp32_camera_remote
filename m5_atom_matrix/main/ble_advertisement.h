#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct { bool hid, gamepad; char name[33]; } ble_advertisement_t;
/* Merge advertising and scan-response fields; malformed fields are rejected. */
bool ble_advertisement_parse(ble_advertisement_t *out, const uint8_t *data, size_t size);
