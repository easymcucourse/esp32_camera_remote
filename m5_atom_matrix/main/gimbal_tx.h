#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { GIMBAL_TX_CONTROL, GIMBAL_TX_NEUTRAL } gimbal_tx_kind_t;
typedef struct {
    unsigned count;
    struct { gimbal_tx_kind_t kind; uint32_t started_ms; } pending[2];
} gimbal_tx_t;
/* At most one ordinary command; one additional slot is reserved for neutral.
 * These slots describe accepted stack writes, never a queue of future targets. */
bool gimbal_tx_available(const gimbal_tx_t *s, gimbal_tx_kind_t kind);
void gimbal_tx_accepted(gimbal_tx_t *s, gimbal_tx_kind_t kind, uint32_t now);
bool gimbal_tx_complete(gimbal_tx_t *s, gimbal_tx_kind_t *kind);
bool gimbal_tx_expired(const gimbal_tx_t *s, uint32_t now);
