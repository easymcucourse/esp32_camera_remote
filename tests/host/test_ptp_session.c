#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ptp_session.h"
#include "ptpip_packet.h"
#include "ptp_codes.h"
#include "lwip/sockets.h"
static unsigned depth;
static bool begin_fails;
bool ptpip_transaction_begin(int fd) { (void)fd; if (begin_fails) return false; ++depth; return true; }
void ptpip_transaction_end(void) { assert(depth); --depth; }
static uint8_t input[1024], sent[1024];
static size_t size, at, sent_size;
static void word(uint8_t *b,uint16_t v) { b[0]=v; b[1]=v>>8; }
static uint8_t *packet(uint32_t type,unsigned n,uint32_t tid) {
    uint8_t *b=input+size; memset(b,0,n); put32(b,n); put32(b+4,type);
    if (n>=12) put32(b+8,tid);
    size+=n; return b;
}
static void response(uint32_t tid,uint16_t code) {
    uint8_t *b=packet(PTPIP_OPERATION_RESPONSE,14,0); word(b+8,code); put32(b+10,tid);
}
static void object(uint32_t tid) {
    uint8_t *b=packet(PTPIP_START_DATA,20,tid); put32(b+12,3);
    b=packet(PTPIP_DATA,14,tid); b[12]=1; b[13]=2;
    b=packet(PTPIP_END_DATA,13,tid); b[12]=3;
    response(tid,PTP_RC_OK);
}
static void reset(void) { assert(!depth); size=at=sent_size=0; begin_fails=false; }
bool ptpip_transfer(int fd,void *b,size_t n,bool write) {
    (void)fd;
    if (write) { assert(sent_size+n<=sizeof(sent)); memcpy(sent+sent_size,b,n); sent_size+=n; return true; }
    if (n>size-at) return false;
    memcpy(b,input+at,n); at+=n; return true;
}
int select(int nfds,fd_set *read,fd_set *write,fd_set *error,struct timeval *wait) {
    (void)nfds;(void)read;(void)write;(void)error;(void)wait; return at<size;
}
static ptp_data_status_t request(uint32_t tid) {
    uint8_t out[16]={0}; size_t n=99; uint16_t code=0;
    ptp_data_status_t result=ptp_request_data_result(3,PTP_OC_GET_OBJECT,tid,NULL,0,out,sizeof(out),&n,&code);
    if (result==PTP_DATA_OK) assert(n==3 && out[0]==1 && out[1]==2 && out[2]==3);
    if (result==PTP_DATA_REFUSED) assert(n==0 && code==PTP_RC_ACCESS_DENIED);
    return result;
}
int main(void) {
    reset(); response(7,PTP_RC_ACCESS_DENIED); object(8);
    assert(request(7)==PTP_DATA_REFUSED); assert(request(8)==PTP_DATA_OK); assert(at==size);
    reset(); packet(PTPIP_PROBE_REQUEST,8,0); object(7);
    assert(request(7)==PTP_DATA_OK); assert(get32(sent+18+4)==PTPIP_PROBE_RESPONSE);
    reset(); response(8,PTP_RC_OK); assert(request(7)==PTP_DATA_PROTOCOL);
    reset(); uint8_t *b=packet(PTPIP_START_DATA,20,7); put32(b+12,3); response(7,PTP_RC_ACCESS_DENIED);
    assert(request(7)==PTP_DATA_PROTOCOL); /* unfinished phase never reusable */
    reset(); b=packet(PTPIP_START_DATA,20,7); put32(b+12,3); b=packet(PTPIP_END_DATA,14,7);
    assert(request(7)==PTP_DATA_PROTOCOL); /* short end */
    reset(); b=packet(PTPIP_START_DATA,20,7); put32(b+12,17); assert(request(7)==PTP_DATA_PROTOCOL);
    reset(); b=packet(PTPIP_START_DATA,20,7); put32(b+12,3); put32(b+16,1); assert(request(7)==PTP_DATA_PROTOCOL);
    reset(); packet(PTPIP_OPERATION_RESPONSE,15,7); assert(request(7)==PTP_DATA_PROTOCOL);
    reset(); object(7); size--; assert(request(7)==PTP_DATA_IO);
    reset(); packet(PTPIP_PROBE_REQUEST,8,0); response(2,PTP_RC_OK);
    assert(ptp_operation(3,PTP_OC_OPEN_SESSION,2,true));
    assert(get32(sent+22+4)==PTPIP_PROBE_RESPONSE);
    reset(); b=packet(PTPIP_EVENT,18,0); word(b+8,0xc203); put32(b+10,0);
    bool changed=false; assert(ptp_drain_events_changed(3,&changed)&&changed);
    reset(); b=packet(PTPIP_EVENT,18,0); word(b+8,0xc207); put32(b+10,0);
    assert(ptp_drain_events_changed(3,&changed)&&!changed);
    reset(); packet(PTPIP_EVENT,15,0); assert(!ptp_drain_events_changed(3,&changed));
    reset(); begin_fails=true;
    assert(request(7)==PTP_DATA_IO && !depth && !sent_size);
    puts("PTP refusal, framing, probe and event tests passed"); return 0;
}
