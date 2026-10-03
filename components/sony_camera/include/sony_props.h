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
/* Borrowed views into a validated snapshot, valid only during the callback.
 * Both observed enum lists are exposed without guessing their meaning. */
typedef struct {
    uint16_t code, type;
    uint8_t getset, enabled, form;
    const uint8_t *current, *choices, *second_choices;
    size_t current_size;
    uint16_t choice_count, second_choice_count;
    bool scalar, writable;
    uint32_t value;
} sony_property_desc_t;
typedef void (*sony_descriptor_visitor_t)(void *context, const sony_property_desc_t *desc);
/* Invalid/truncated/unsupported datasets emit nothing. Unknown property codes
 * with known PTP types are safely consumed. No allocation or byte searching. */
bool sony_parse_descriptors(const uint8_t *data, size_t size,
                            sony_descriptor_visitor_t visit, void *context);
bool sony_descriptor_choice(const sony_property_desc_t *desc, unsigned index, uint32_t *value);
/* All state belongs to the socket owner. Invalid input clears mode capability. */
void sony_parse_properties(const uint8_t *data, size_t size, sony_mode_state_t *mode,
                           sony_property_visitor_t visit, void *context);
typedef struct {
    bool focus_known, zoom_known;
    bool recording_known, recording;
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
