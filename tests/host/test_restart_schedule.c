#include "restart_schedule.h"
#include <assert.h>
int main(void)
{
    restart_schedule_t s={0};assert(!restart_schedule_due(&s,0));assert(!restart_schedule_commit(&s,0,1500));
    assert(restart_schedule_prepare(&s));assert(!restart_schedule_prepare(&s));
    assert(!restart_schedule_commit(&s,0,10001));assert(restart_schedule_cancel(&s));
    assert(restart_schedule_prepare(&s));assert(restart_schedule_commit(&s,100,1500));
    assert(!restart_schedule_commit(&s,100,1500));assert(!restart_schedule_cancel(&s));
    assert(!restart_schedule_due(&s,1599));assert(restart_schedule_due(&s,1600));
    s=(restart_schedule_t){0};assert(restart_schedule_prepare(&s));
    assert(restart_schedule_commit(&s,UINT32_MAX-1000,1500));
    assert(!restart_schedule_due(&s,498));assert(restart_schedule_due(&s,499));return 0;
}
