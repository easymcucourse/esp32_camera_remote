#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint32_t values[64], current;
    unsigned count;
    bool writable;
} sony_mode_state_t;
typedef void (*sony_property_visitor_t)(void *context, uint16_t code, uint32_t value);
// Retains the existing byte-search parser. All state belongs to the socket owner.
// The visitor is called synchronously for properties found in the input buffer.
void sony_parse_properties(const uint8_t *data, size_t size, sony_mode_state_t *mode,
                           sony_property_visitor_t visit, void *context);
typedef struct {
    bool focus_known, zoom_known;
    uint16_t focus_mode;
    uint8_t zoom_enabled;
} sony_focus_caps_t;
/* Complete sequential validation, single/dual enum-list datasets.
 * Missing properties stay unknown; partial/malformed data never enables MF. */
bool sony_parse_focus_caps(const uint8_t *data, size_t size, sony_focus_caps_t *out);
/* Validates first, then visits integer scalar current values (up to 32 bits).
 * Signed values retain their wire bit pattern. Invalid datasets emit nothing. */
bool sony_parse_scalar_properties(const uint8_t *data, size_t size,
                                  sony_property_visitor_t visit, void *context);
