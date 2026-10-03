#pragma once
#include "ds4_report.h"
/* Profile captured from the user's Ultimate 2 BLE HID service. Other report
 * maps are rejected rather than interpreting arbitrary HID as camera keys. */
extern const uint8_t ultimate2_report_map[113];
bool ultimate2_map_matches(const uint8_t *map, size_t size);
bool ultimate2_parse_report(const uint8_t *data, size_t size, ds4_state_t *out);
