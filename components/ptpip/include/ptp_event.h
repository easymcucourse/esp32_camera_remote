#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool probe; uint16_t code; uint32_t transaction, params[3]; unsigned count; } ptpip_event_t;
