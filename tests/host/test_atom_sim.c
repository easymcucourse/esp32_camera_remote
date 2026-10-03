#include "atom_sim.h"
#include "atom_client.h"
#include <assert.h>
static atom_client_result_t transact(atom_sim_t *s,atom_client_t *c,const uint8_t **p,uint8_t *reply)
{
    uint8_t request[ATOM_REQUEST_SIZE];size_t size;
    atom_encode_request(request,atom_client_request(c));
    return atom_sim_transact(s,request,reply,&size)==ATOM_SIM_OK?
        atom_client_response(c,reply,size,p):atom_client_failure(c);
}
int main(void)
{
    atom_sim_t s;atom_sim_init(&s);atom_client_t c={0};uint8_t reply[ATOM_RESPONSE_MAX];const uint8_t *p;
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_HELLO);
    pad_state_t pad={.connected=true,.battery=7};atom_sim_pad(&s,&pad);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_POLL && p[8]==7 && (p[27]&ATOM_DEBUG_SIM));
    pad.buttons=1u<<3;atom_sim_pad(&s,&pad);atom_sim_gap(&s,true);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_POLL && p[18]==3);
    uint32_t id=atom_read_le(p+19,4);assert(id);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_POLL && atom_read_le(p+19,4)==id);
    c.ack_id=id;assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_POLL && !p[18] && !s.events.gap_pending);
    s.crc=2; uint8_t seq=c.seq;
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_RETRY && c.seq==seq && c.online);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_RETRY && c.seq==seq && c.online);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_POLL && !c.failures);
    s.fail=3;
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_RETRY);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_RETRY);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_OFFLINE && !c.ack_id);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_HELLO);
    atom_sim_reboot(&s);assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_RESTART);
    assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_HELLO);
    s.version=1;assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_MISMATCH);
    s.version=2;assert(transact(&s,&c,&p,reply)==ATOM_CLIENT_HELLO);
    s.online=false;s.timeout=2;
    uint8_t req[ATOM_REQUEST_SIZE];size_t size;atom_encode_request(req,atom_client_request(&c));
    assert(atom_sim_transact(&s,req,reply,&size)==ATOM_SIM_TIMEOUT && !size && s.timeout==2);
    s.online=true;assert(atom_sim_transact(&s,req,reply,&size)==ATOM_SIM_TIMEOUT && s.timeout==1);
    pad.connected=false;atom_sim_pad(&s,&pad);assert(s.pad.buttons==0 && s.pad.battery==255);
    return 0;
}
