#pragma once
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
typedef struct { atomic_uint drop, corrupt, delay_ms; } atom_fault_t;
typedef struct { bool drop, corrupt; unsigned delay_ms; } atom_fault_plan_t;
/* Called only for legal requests; drop has priority and preserves corrupt count. */
atom_fault_plan_t atom_fault_take(atom_fault_t *fault);
