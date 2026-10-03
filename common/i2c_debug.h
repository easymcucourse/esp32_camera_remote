#pragma once
#include "i2c_monitor.h"
/* Task context only. Each firmware has its own bounded monitor. */
void i2c_debug_record(const i2c_frame_record_t *record);
bool i2c_debug_command(int argc,char **argv);
void i2c_debug_poll(void);
