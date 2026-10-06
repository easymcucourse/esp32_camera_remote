#include "i2c_monitor.h"

const char *i2c_monitor_result_name(i2c_mon_result_t r)
{
    static const char *const names[]={"OK","TIMEOUT","BAD_CRC","BAD_HEADER","BAD_SEQ","REMOTE","IO","BAD_PARAM"};
    return (unsigned)r<I2C_MON_RESULT_COUNT?names[r]:"UNKNOWN";
}
