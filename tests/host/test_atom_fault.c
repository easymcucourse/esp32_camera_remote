#include "atom_fault.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    atom_fault_t f={0}; atom_fault_plan_t p=atom_fault_take(&f);
    assert(!p.drop && !p.corrupt && !p.delay_ms);
    atomic_store(&f.drop,2); atomic_store(&f.corrupt,3); atomic_store(&f.delay_ms,100);
    for (unsigned i=0;i<2;++i) { p=atom_fault_take(&f); assert(p.drop && !p.corrupt && !p.delay_ms); }
    assert(!atomic_load(&f.drop) && atomic_load(&f.corrupt)==3);
    for (unsigned i=0;i<3;++i) { p=atom_fault_take(&f); assert(!p.drop && p.corrupt && p.delay_ms==100); }
    p=atom_fault_take(&f); assert(!p.drop && !p.corrupt && p.delay_ms==100);
    atomic_store(&f.delay_ms,0); atomic_store(&f.drop,1);
    assert(atom_fault_take(&f).drop); assert(!atom_fault_take(&f).drop);
    puts("Fault priority, finite counters and persistent delay reset passed");
}
