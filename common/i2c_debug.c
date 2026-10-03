#include "i2c_debug.h"
#include "debug_console.h"
#include "freertos/FreeRTOS.h"
#include <string.h>
#include <stdio.h>
static portMUX_TYPE mutex=portMUX_INITIALIZER_UNLOCKED;
static i2c_monitor_t monitor;
static uint32_t reported_dropped;
void i2c_debug_record(const i2c_frame_record_t *record)
{ portENTER_CRITICAL(&mutex); i2c_monitor_record(&monitor,record); portEXIT_CRITICAL(&mutex); }
bool i2c_debug_command(int argc,char **argv)
{
    if (strcmp(argv[0],"i2c")) return false;
    if (argc==3 && !strcmp(argv[1],"log")) {
        i2c_log_mode_t mode;
        if (!strcmp(argv[2],"on")) mode=I2C_LOG_ALL;
        else if (!strcmp(argv[2],"off")) mode=I2C_LOG_OFF;
        else if (!strcmp(argv[2],"changes")) mode=I2C_LOG_CHANGES;
        else { debug_printf("[dbg] ERR usage: i2c log on|off|changes\n"); return true; }
        portENTER_CRITICAL(&mutex); i2c_monitor_mode(&monitor,mode); portEXIT_CRITICAL(&mutex);
        debug_printf("[dbg] OK i2c log %s\n",argv[2]); return true;
    }
    if ((argc==2 || (argc==3 && !strcmp(argv[2],"reset"))) && !strcmp(argv[1],"stats")) {
        i2c_monitor_stats_t stats;
        portENTER_CRITICAL(&mutex);
        if (argc==3) { i2c_monitor_reset_stats(&monitor); reported_dropped=0; }
        stats=monitor.stats;
        portEXIT_CRITICAL(&mutex);
        debug_printf("[dbg] OK i2c stats total=%lu failed=%lu timeout=%lu bad_crc=%lu bad_header=%lu bad_seq=%lu remote=%lu io=%lu bad_param=%lu max_ms=%lu log_dropped=%lu\n",
            (unsigned long)stats.total,(unsigned long)stats.failed,(unsigned long)stats.counts[I2C_MON_TIMEOUT],
            (unsigned long)stats.counts[I2C_MON_BAD_CRC],(unsigned long)stats.counts[I2C_MON_BAD_HEADER],
            (unsigned long)stats.counts[I2C_MON_BAD_SEQ],(unsigned long)stats.counts[I2C_MON_REMOTE],
            (unsigned long)stats.counts[I2C_MON_IO],(unsigned long)stats.counts[I2C_MON_BAD_PARAM],
            (unsigned long)stats.max_ms,(unsigned long)stats.log_dropped); return true;
    }
    return false; /* Other platform i2c commands can follow. */
}
static void hex(char *out,const uint8_t *bytes,unsigned n)
{
    static const char digits[]="0123456789abcdef";
    for (unsigned i=0;i<n;++i) { out[3*i]=digits[bytes[i]>>4]; out[3*i+1]=digits[bytes[i]&15]; out[3*i+2]=' '; }
    out[3*n]=0;
}
void i2c_debug_poll(void)
{
    /* Limit each poll so UART command consumption can't starve behind logging. */
    for (unsigned i=0;i<2;++i) {
        i2c_frame_record_t record; uint32_t dropped;
        portENTER_CRITICAL(&mutex);
        bool valid=i2c_monitor_read(&monitor,&record); dropped=monitor.stats.log_dropped;
        portEXIT_CRITICAL(&mutex);
        if (dropped!=reported_dropped) {
            debug_printf("[dbg] i2c log dropped=%lu\n",(unsigned long)(dropped-reported_dropped)); reported_dropped=dropped;
        }
        if (!valid) return;
        char req[ATOM_REQUEST_SIZE*3+1],resp[ATOM_RESPONSE_MAX*3+1];
        hex(req,record.request,ATOM_REQUEST_SIZE); hex(resp,record.response,record.response_len);
        debug_printf("[dbg] i2c %lu #%02x %s > %s< %s%s %lums\n",(unsigned long)record.timestamp,
            record.request[2],record.request[3]==ATOM_CMD_POLL?"POLL":record.request[3]==ATOM_CMD_HELLO?"HELLO":"UNKNOWN",
            req,resp,i2c_monitor_result_name(record.result),(unsigned long)record.elapsed_ms);
    }
}
