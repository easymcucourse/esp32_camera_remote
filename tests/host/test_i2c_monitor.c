#include "i2c_monitor.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static i2c_frame_record_t frame(unsigned seq,unsigned battery)
{
    i2c_frame_record_t r={.timestamp=100,.elapsed_ms=16,.result=I2C_MON_OK};
    atom_request_t req={(uint8_t)seq,ATOM_CMD_POLL,0};
    uint8_t payload[ATOM_POLL_SIZE]={0}; payload[8]=battery;
    atom_encode_request(r.request,req);
    r.response_len=atom_encode_response(r.response,req,ATOM_OK,payload,sizeof(payload));
    return r;
}
int main(void)
{
    i2c_monitor_t m={0}; i2c_frame_record_t r=frame(1,5),out;
    i2c_monitor_record(&m,&r); assert(m.stats.total==1 && !m.count && !m.stats.failed);
    i2c_monitor_mode(&m,I2C_LOG_CHANGES); i2c_monitor_record(&m,&r);
    r=frame(2,5); r.timestamp=200; r.elapsed_ms=20; i2c_monitor_record(&m,&r);
    assert(m.count==1 && m.stats.total==3 && m.stats.max_ms==20);
    r=frame(3,6); i2c_monitor_record(&m,&r); assert(m.count==2);
    atom_request_t req={3,ATOM_CMD_POLL,99}; atom_encode_request(r.request,req);
    i2c_monitor_record(&m,&r); assert(m.count==3); /* ACK is semantically meaningful. */
    r.result=I2C_MON_BAD_CRC; i2c_monitor_record(&m,&r); i2c_monitor_record(&m,&r);
    assert(m.count==5 && m.stats.failed==2 && m.stats.counts[I2C_MON_BAD_CRC]==2);
    i2c_monitor_mode(&m,I2C_LOG_ALL);
    for (unsigned i=0;i<I2C_MONITOR_CAPACITY;++i) { r=frame(i,5); i2c_monitor_record(&m,&r); }
    i2c_monitor_record(&m,&r); assert(m.count==32 && m.stats.log_dropped==1);
    for (unsigned i=0;i<32;++i) { assert(i2c_monitor_read(&m,&out)); assert(out.request[2]==i); }
    assert(!i2c_monitor_read(&m,&out));
    for (unsigned i=0;i<70;++i) { r=frame(i,5); i2c_monitor_record(&m,&r); assert(i2c_monitor_read(&m,&out) && out.request[2]==i); }
    i2c_monitor_record(&m,&r); i2c_monitor_reset_stats(&m);
    assert(!m.stats.total && !m.stats.failed && !m.stats.max_ms && !m.stats.log_dropped && m.count==1);
    i2c_monitor_mode(&m,I2C_LOG_OFF); assert(!m.count);
    r.response_len=36; i2c_monitor_record(&m,&r); assert(!m.stats.total);
    r=frame(3,5); req=(atom_request_t){3,ATOM_CMD_POLL,0};
    for (unsigned n=0;n<r.response_len;++n) assert(i2c_monitor_response(r.response,n,req,28)==I2C_MON_BAD_HEADER);
    assert(i2c_monitor_response(r.response,35,req,28)==I2C_MON_OK);
    r.response[2]^=1; assert(i2c_monitor_response(r.response,35,req,28)==I2C_MON_BAD_SEQ); r.response[2]^=1;
    r.response[34]^=1; assert(i2c_monitor_response(r.response,35,req,28)==I2C_MON_BAD_CRC); r.response[34]^=1;
    r.response[5]=255; assert(i2c_monitor_response(r.response,35,req,28)==I2C_MON_BAD_HEADER);
    r.response_len=atom_encode_response(r.response,req,ATOM_NOT_READY,NULL,0);
    memset(r.response+7,0xff,28); assert(i2c_monitor_response(r.response,35,req,28)==I2C_MON_REMOTE);
    puts("I2C classifications, semantic changes, failure retention, ring bounds and reset passed");
}
