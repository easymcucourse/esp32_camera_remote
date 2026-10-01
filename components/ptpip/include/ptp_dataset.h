#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Outputs are valid only on success; each caller-owned buffer has 24 bytes.
bool ptp_parse_device_info(const uint8_t *data, size_t size,
                           char model[24], char firmware[24]);
