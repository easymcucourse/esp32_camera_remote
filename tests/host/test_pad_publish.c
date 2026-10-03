#include "pad_publish.h"
#include <assert.h>
#include "atom_protocol.h"
int main(void)
{
    assert(pad_publish_source(false,false)==PAD_SOURCE_NONE);
    assert(pad_publish_source(false,true)==PAD_SOURCE_BLE);
    assert(pad_publish_source(true,true)==PAD_SOURCE_DS4);
    assert(pad_publish_mode_source(ATOM_INPUT_DS,false,true)==PAD_SOURCE_NONE);
    assert(pad_publish_mode_source(ATOM_INPUT_DS,true,true)==PAD_SOURCE_DS4);
    assert(pad_publish_mode_source(ATOM_INPUT_XBOX,true,false)==PAD_SOURCE_NONE);
    assert(pad_publish_mode_source(ATOM_INPUT_XBOX,true,true)==PAD_SOURCE_BLE);
    assert(pad_publish_mode_source(2,true,true)==PAD_SOURCE_NONE);
    pad_publish_t p={0};ds4_state_t s={.connected=true,.buttons=1u<<14,.battery=255};
    pad_publish_apply(&p,PAD_SOURCE_BLE,&s);
    assert(p.state.buttons==s.buttons && !p.events.count); /* held on connect */
    s.buttons=0;pad_publish_apply(&p,PAD_SOURCE_BLE,&s);
    s.buttons=1u<<12;pad_publish_apply(&p,PAD_SOURCE_BLE,&s);
    assert(p.events.count==2);uint32_t id=p.events.next_id;uint8_t gen=p.generation;
    s.buttons=1u<<13;pad_publish_apply(&p,PAD_SOURCE_DS4,&s);
    assert(p.generation!=gen && !p.events.count && p.events.next_id==id);
    pad_publish_apply(&p,PAD_SOURCE_DS4,&s);assert(!p.events.count);
    s.connected=false;s.r2=255;pad_publish_apply(&p,PAD_SOURCE_NONE,&s);
    assert(!p.state.connected && !p.state.buttons && !p.state.r2 && p.state.battery==255 && !p.events.count);
    s.connected=true;pad_publish_apply(&p,PAD_SOURCE_BLE,&s);assert(!p.events.count);
    p.events.dropped=7;pad_publish_reset(&p);
    assert(p.events.next_id==id && p.events.dropped==7 && !p.state.connected);
    s.buttons=0;pad_publish_apply(&p,PAD_SOURCE_SIM,&s);
    s.buttons=1u<<3;pad_publish_apply(&p,PAD_SOURCE_SIM,&s);assert(p.events.count==1);
    pad_publish_reset(&p);pad_publish_apply(&p,PAD_SOURCE_BLE,&s);assert(!p.events.count);
    return 0;
}
