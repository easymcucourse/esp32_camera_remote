#include "gimbal_tx.h"
#include <assert.h>
int main(void)
{
    gimbal_tx_t s={0}; gimbal_tx_kind_t kind;
    assert(!gimbal_tx_complete(&s,&kind) && !gimbal_tx_expired(&s,1000));
    assert(gimbal_tx_available(&s,GIMBAL_TX_CONTROL));
    gimbal_tx_accepted(&s,GIMBAL_TX_CONTROL,1000);
    for (unsigned i=0;i<1000;++i) {
        assert(!gimbal_tx_available(&s,GIMBAL_TX_CONTROL));
        gimbal_tx_accepted(&s,GIMBAL_TX_CONTROL,1001+i);
        assert(s.count==1 && s.pending[0].started_ms==1000);
    }
    assert(gimbal_tx_available(&s,GIMBAL_TX_NEUTRAL));
    gimbal_tx_accepted(&s,GIMBAL_TX_NEUTRAL,1040);
    assert(s.count==2 && !gimbal_tx_available(&s,GIMBAL_TX_CONTROL) && !gimbal_tx_available(&s,GIMBAL_TX_NEUTRAL));
    assert(!gimbal_tx_expired(&s,1499) && gimbal_tx_expired(&s,1500));
    assert(gimbal_tx_complete(&s,&kind) && kind==GIMBAL_TX_CONTROL && s.count==1);
    assert(!gimbal_tx_available(&s,GIMBAL_TX_CONTROL) && !gimbal_tx_available(&s,GIMBAL_TX_NEUTRAL));
    assert(!gimbal_tx_expired(&s,1500) && gimbal_tx_expired(&s,1540));
    assert(gimbal_tx_complete(&s,&kind) && kind==GIMBAL_TX_NEUTRAL && !s.count);
    gimbal_tx_accepted(&s,GIMBAL_TX_NEUTRAL,UINT32_MAX-100);
    assert(!gimbal_tx_expired(&s,398) && gimbal_tx_expired(&s,399));
    assert(gimbal_tx_complete(&s,0));
    return 0;
}
