#include "maint_notice.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    maint_notice_t s={0};
    assert(!maint_notice_tick(&s, 0));
    maint_notice_set(&s, MAINT_NOTICE_ERROR, UINT32_MAX-1000);
    assert(maint_notice_tick(&s, UINT32_MAX));
    assert(maint_notice_tick(&s, 1998));
    assert(!maint_notice_tick(&s, 1999));
    assert(s.kind == MAINT_NOTICE_NONE);
    maint_notice_set(&s, MAINT_NOTICE_CAMERA, 5000);
    assert(maint_notice_tick(&s, 7999));
    maint_notice_set(&s, MAINT_NOTICE_ERROR, 7999);
    assert(maint_notice_tick(&s, 8000));
    assert(s.kind == MAINT_NOTICE_ERROR);
    assert(!maint_notice_tick(&s, 10999));
    maint_notice_set(&s, MAINT_NOTICE_CAMERA, 11000);
    maint_notice_set(&s, MAINT_NOTICE_NONE, 11001);
    assert(!maint_notice_tick(&s, 11002));
    puts("Maintenance notice lifetime and wraparound passed");
    return 0;
}
