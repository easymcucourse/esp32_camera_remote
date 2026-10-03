#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct { uint32_t deadline,epoch;bool armed; } maint_confirm_t;
static inline void maint_confirm_cancel(maint_confirm_t *s) { s->armed=false; }
static inline void maint_confirm_tick(maint_confirm_t *s,uint32_t now,uint32_t epoch)
{ if (s->armed && (s->epoch!=epoch || (int32_t)(now-s->deadline)>=0)) s->armed=false; }
static inline bool maint_confirm_press(maint_confirm_t *s,uint32_t now,uint32_t epoch)
{
    maint_confirm_tick(s,now,epoch);
    if (s->armed) { s->armed=false;return true; }
    *s=(maint_confirm_t){.armed=true,.deadline=now+3000,.epoch=epoch};return false;
}
