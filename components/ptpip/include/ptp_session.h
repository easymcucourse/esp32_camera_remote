#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Call only from the socket owner, between complete transactions.
int ptp_receive_packet(int fd, uint8_t *packet, size_t capacity);
bool ptp_operation(int fd, uint16_t code, uint32_t transaction, bool session_param);
bool ptp_request_data(int fd, uint16_t opcode, uint32_t transaction,
                  const uint32_t *params, unsigned num_params,
                  uint8_t *output, size_t capacity, size_t *output_size);
bool ptp_drain_events(int event);

/* REFUSED means a complete, synchronized transaction; IO/PROTOCOL require closing. */
typedef enum { PTP_DATA_OK, PTP_DATA_REFUSED, PTP_DATA_IO, PTP_DATA_PROTOCOL } ptp_data_status_t;
ptp_data_status_t ptp_request_data_result(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity,
    size_t *output_size, uint16_t *response);
bool ptp_drain_events_changed(int event, bool *properties_changed);
