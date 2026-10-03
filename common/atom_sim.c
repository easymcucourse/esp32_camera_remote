#include "atom_sim.h"
#include <string.h>
void atom_sim_init(atom_sim_t *s)
{
    *s=(atom_sim_t){.boot_id=1,.version=2,.online=true,.pad.battery=255};
}
void atom_sim_pad(atom_sim_t *s, const pad_state_t *pad)
{
    bool changed=s->pad.connected!=pad->connected;
    s->pad=*pad;
    if (!pad->connected) s->pad=(pad_state_t){.battery=255};
    if (changed) {
        s->generation=(s->generation+1)&127;
        s->events=(ds4_events_t){.last_buttons=s->pad.buttons & ATOM_BUTTON_MASK & ~ATOM_LOCAL_MASK};
    } else ds4_events_push(&s->events,s->pad.buttons);
}
void atom_sim_reboot(atom_sim_t *s)
{
    uint32_t boot=s->boot_id+1; if (!boot) boot=1;
    atom_sim_init(s); s->boot_id=boot;
}
void atom_sim_gap(atom_sim_t *s, bool overflow)
{
    s->events.gap_pending=true;
    if (overflow) ++s->events.dropped;
}
atom_sim_result_t atom_sim_transact(atom_sim_t *s, const uint8_t raw[ATOM_REQUEST_SIZE],
                                  uint8_t reply[ATOM_RESPONSE_MAX], size_t *size)
{
    *size=0;
    if (!s->online) return ATOM_SIM_TIMEOUT;
    if (s->fail) { --s->fail; return ATOM_SIM_IO; }
    if (s->timeout) { --s->timeout; return ATOM_SIM_TIMEOUT; }
    atom_receiver_t parser={0}; atom_request_t request={0}; atom_status_t status=ATOM_BAD_PARAM;
    bool complete=false;
    for (unsigned i=0;i<ATOM_REQUEST_SIZE;++i)
        if (atom_receiver_feed(&parser,raw[i],0,&request,&status)) { complete=true; break; }
    if (!complete) return ATOM_SIM_IO;
    uint8_t payload[ATOM_POLL_SIZE]={0}, length=0;
    if (status==ATOM_OK) {
        if (request.cmd==ATOM_CMD_HELLO) {
            if (request.param&0xffff0000 || (request.param&255)>2 || ((request.param>>8)&255)<2)
                status=ATOM_BAD_VERSION;
            else {
                atom_write_le(payload,s->boot_id,4); payload[4]=s->version;
                payload[6]=9; payload[7]=DS4_EVENT_CAPACITY;
                atom_write_le(payload+8,ATOM_LOCAL_MASK,3); length=ATOM_HELLO_SIZE;
            }
        } else if (request.cmd==ATOM_CMD_POLL) {
            ds4_event_t event; bool valid=ds4_events_read(&s->events,request.param,&event);
            atom_write_le(payload,s->boot_id,4);
            payload[4]=s->pad.connected?3:0; payload[5]=s->gimbal;
            payload[7]=s->events.gap_pending?1:0;
            payload[8]=s->pad.connected?s->pad.battery:255;
            atom_write_le(payload+11,s->pad.buttons & ATOM_BUTTON_MASK & ~ATOM_LOCAL_MASK,3);
            payload[14]=(uint8_t)s->pad.rx; payload[15]=(uint8_t)s->pad.ry;
            payload[16]=s->pad.l2; payload[17]=s->pad.r2;
            if (valid) {
                payload[18]=1|(event.gap?2:0); atom_write_le(payload+19,event.id,4);
                atom_write_le(payload+23,event.buttons,3); payload[26]=s->events.count-1;
            }
            payload[27]=ATOM_DEBUG_SIM|(s->generation<<1); length=ATOM_POLL_SIZE;
        } else status=ATOM_UNKNOWN_CMD;
    }
    *size=atom_encode_response(reply,request,status,payload,length);
    reply[1]=s->version; reply[*size-1]=atom_crc8(reply,*size-1);
    if (s->crc) { --s->crc; reply[*size-1]^=255; }
    return ATOM_SIM_OK;
}
