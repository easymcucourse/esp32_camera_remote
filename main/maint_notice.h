#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { MAINT_NOTICE_NONE, MAINT_NOTICE_ERROR, MAINT_NOTICE_CAMERA } maint_notice_kind_t;
typedef struct { maint_notice_kind_t kind; uint32_t started; } maint_notice_t;

static inline void maint_notice_set(maint_notice_t *s, maint_notice_kind_t kind, uint32_t now)
{ *s = (maint_notice_t){.kind=kind, .started=now}; }
static inline bool maint_notice_tick(maint_notice_t *s, uint32_t now)
{
    if (s->kind == MAINT_NOTICE_NONE) return false;
    if ((uint32_t)(now-s->started) < 3000) return true;
    s->kind=MAINT_NOTICE_NONE;
    return false;
}
