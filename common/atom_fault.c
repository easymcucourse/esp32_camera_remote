#include "atom_fault.h"
static bool consume(atomic_uint *value)
{
    unsigned count=atomic_load(value);
    while (count && !atomic_compare_exchange_weak(value,&count,count-1)) {}
    return count!=0;
}
atom_fault_plan_t atom_fault_take(atom_fault_t *f)
{
    if (consume(&f->drop)) return (atom_fault_plan_t){.drop=true};
    return (atom_fault_plan_t){.corrupt=consume(&f->corrupt),.delay_ms=atomic_load(&f->delay_ms)};
}
