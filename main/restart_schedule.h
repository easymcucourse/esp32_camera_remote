#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { bool prepared,committed;uint32_t deadline; } restart_schedule_t;
static inline bool restart_schedule_prepare(restart_schedule_t *s)
{ if (s->prepared) return false;s->prepared=true;return true; }
static inline bool restart_schedule_commit(restart_schedule_t *s,uint32_t now,unsigned delay)
{
    if (!s->prepared || s->committed || delay>10000) return false;
    s->deadline=now+delay;s->committed=true;return true;
}
static inline bool restart_schedule_cancel(restart_schedule_t *s)
{ if (s->committed) return false;*s=(restart_schedule_t){0};return true; }
static inline bool restart_schedule_due(const restart_schedule_t *s,uint32_t now)
{ return s->committed && (int32_t)(now-s->deadline)>=0; }
