#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pad_player.h"

/* Same canonical snapshot for real reports and development simulation.
 * Battery 0..10 or 255 unknown; bits follow atom_protocol / DS4 mapping. */
typedef pad_state_t ds4_state_t;

bool ds4_parse_report(const uint8_t *data, size_t length, ds4_state_t *out);
