#include "i2c_monitor.h"
#include <string.h>
i2c_mon_result_t i2c_monitor_response(const uint8_t *b,size_t size,atom_request_t req,uint8_t length)
{
    if (!b || size<7 || b[0]!=0x5a || b[1]!=ATOM_PROTOCOL_VERSION || b[3]!=req.cmd ||
        b[4]>ATOM_NOT_READY || b[5]>ATOM_POLL_SIZE || (size_t)b[5]+7>size ||
        (b[4]==ATOM_OK?b[5]!=length:b[5]!=0)) return I2C_MON_BAD_HEADER;
    if (b[2]!=req.seq) return I2C_MON_BAD_SEQ;
    if (atom_crc8(b,6+b[5])!=b[6+b[5]]) return I2C_MON_BAD_CRC;
    return b[4]==ATOM_OK?I2C_MON_OK:I2C_MON_REMOTE;
}
void i2c_monitor_mode(i2c_monitor_t *m,i2c_log_mode_t mode)
{ m->mode=mode; m->head=m->count=0; m->have_previous=false; }
static bool equivalent(const i2c_frame_record_t *a,const i2c_frame_record_t *b)
{
    /* A poll's sequence and its dependent CRC change every time. Compare the
     * semantic fields so changes mode actually suppresses idle traffic. */
    if (a->response_len!=b->response_len || memcmp(a->request,b->request,2) ||
        memcmp(a->request+3,b->request+3,5)) return false;
    if (a->response_len<7) return false;
    return !memcmp(a->response,b->response,2) &&
        !memcmp(a->response+3,b->response+3,a->response_len-4);
}
void i2c_monitor_record(i2c_monitor_t *m,const i2c_frame_record_t *record)
{
    if (record->response_len>ATOM_RESPONSE_MAX || (unsigned)record->result>=I2C_MON_RESULT_COUNT) return;
    ++m->stats.total; ++m->stats.counts[record->result];
    if (record->result!=I2C_MON_OK) ++m->stats.failed;
    if (record->elapsed_ms>m->stats.max_ms) m->stats.max_ms=record->elapsed_ms;
    if (m->mode==I2C_LOG_OFF) return;
    if (m->mode==I2C_LOG_CHANGES && record->result==I2C_MON_OK && m->have_previous &&
        m->previous.result==I2C_MON_OK && equivalent(record,&m->previous)) return;
    if (m->count==I2C_MONITOR_CAPACITY) { ++m->stats.log_dropped; return; }
    m->records[(m->head+m->count)%I2C_MONITOR_CAPACITY]=*record; ++m->count;
    m->previous=*record; m->have_previous=true;
}
bool i2c_monitor_read(i2c_monitor_t *m,i2c_frame_record_t *out)
{
    if (!m->count) return false;
    *out=m->records[m->head]; m->head=(m->head+1)%I2C_MONITOR_CAPACITY; --m->count; return true;
}
void i2c_monitor_reset_stats(i2c_monitor_t *m) { m->stats=(i2c_monitor_stats_t){0}; }
