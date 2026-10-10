#include "gimbal_tx.h"

bool gimbal_tx_available(const gimbal_tx_t *s, gimbal_tx_kind_t kind)
{
    if (!s->count) return true;
    return kind==GIMBAL_TX_NEUTRAL && s->count==1 && s->pending[0].kind==GIMBAL_TX_CONTROL;
}
void gimbal_tx_accepted(gimbal_tx_t *s, gimbal_tx_kind_t kind, uint32_t now)
{
    if (!gimbal_tx_available(s,kind)) return;
    s->pending[s->count].kind=kind; s->pending[s->count].started_ms=now; ++s->count;
}
bool gimbal_tx_complete(gimbal_tx_t *s, gimbal_tx_kind_t *kind)
{
    if (!s->count) return false;
    if (kind) *kind=s->pending[0].kind;
    if (--s->count) s->pending[0]=s->pending[1];
    return true;
}
bool gimbal_tx_expired(const gimbal_tx_t *s, uint32_t now)
{ return s->count && (uint32_t)(now-s->pending[0].started_ms)>=500; }
