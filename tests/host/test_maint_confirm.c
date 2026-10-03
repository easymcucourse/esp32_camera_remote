#include "maint_confirm.h"
#include <assert.h>
int main(void)
{
    maint_confirm_t s={0};
    assert(!maint_confirm_press(&s,10,1));assert(s.armed);
    assert(maint_confirm_press(&s,3009,1));assert(!s.armed);
    assert(!maint_confirm_press(&s,4000,1));
    assert(!maint_confirm_press(&s,7000,1)); /* Boundary expires; new first press. */
    assert(!maint_confirm_press(&s,7001,2)); /* Navigation/gap epoch invalidates. */
    maint_confirm_cancel(&s);assert(!maint_confirm_press(&s,7002,2));
    maint_confirm_cancel(&s);assert(!maint_confirm_press(&s,UINT32_MAX-1000,4));
    maint_confirm_tick(&s,1998,4);assert(s.armed);
    maint_confirm_tick(&s,1999,4);assert(!s.armed);
    assert(!maint_confirm_press(&s,2000,4));maint_confirm_tick(&s,2001,5);assert(!s.armed);
    return 0;
}
