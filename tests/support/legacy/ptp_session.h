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
#include "ptp_result.h"
ptp_data_status_t ptp_request_data_result(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned num_params, uint8_t *output, size_t capacity,
    size_t *output_size, uint16_t *response);
bool ptp_drain_events_changed(int event, bool *properties_changed);
/* Temporary old producer entry; delete with other fd APIs after migration. */
bool ptp_send_data(int fd, uint32_t transaction, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted);
int ptp_initialization_exchange(int fd, uint8_t *packet, size_t request_length, size_t capacity);
