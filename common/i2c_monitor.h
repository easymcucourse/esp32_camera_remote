#pragma once
#include "atom_protocol.h"
enum { I2C_MONITOR_CAPACITY = 32 };
typedef enum { I2C_MON_OK, I2C_MON_TIMEOUT, I2C_MON_BAD_CRC, I2C_MON_BAD_HEADER,
    I2C_MON_BAD_SEQ, I2C_MON_REMOTE, I2C_MON_IO, I2C_MON_BAD_PARAM, I2C_MON_RESULT_COUNT } i2c_mon_result_t;
typedef enum { I2C_LOG_OFF, I2C_LOG_ALL, I2C_LOG_CHANGES } i2c_log_mode_t;
typedef struct {
    uint32_t timestamp, elapsed_ms;
    uint8_t request[ATOM_REQUEST_SIZE], response[ATOM_RESPONSE_MAX];
    uint8_t response_len;
    i2c_mon_result_t result;
} i2c_frame_record_t;
typedef struct { uint32_t total, failed, counts[I2C_MON_RESULT_COUNT], max_ms, log_dropped; } i2c_monitor_stats_t;
typedef struct {
    i2c_frame_record_t records[I2C_MONITOR_CAPACITY], previous;
    i2c_monitor_stats_t stats;
    unsigned head, count;
    bool have_previous;
    i2c_log_mode_t mode;
} i2c_monitor_t;
/* Caller serializes these bounded memory operations, never UART work. */
void i2c_monitor_mode(i2c_monitor_t *m, i2c_log_mode_t mode);
void i2c_monitor_record(i2c_monitor_t *m, const i2c_frame_record_t *record);
bool i2c_monitor_read(i2c_monitor_t *m, i2c_frame_record_t *out);
void i2c_monitor_reset_stats(i2c_monitor_t *m);
i2c_mon_result_t i2c_monitor_response(const uint8_t *bytes, size_t size, atom_request_t request, uint8_t payload_len);
const char *i2c_monitor_result_name(i2c_mon_result_t result);
