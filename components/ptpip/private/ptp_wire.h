#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ptp_result.h"
#include "ptp_event.h"
/* Sole standard wire engine. No vendor/network/session state. */
typedef struct {
    void *context;
    bool (*transfer)(void *context, void *buffer, size_t length, bool transmit);
    bool (*begin)(void *context);
    void (*end)(void *context);
} ptp_wire_io_t;
int ptp_wire_receive_packet(const ptp_wire_io_t *io, uint8_t *packet, size_t capacity);
bool ptp_wire_operation(const ptp_wire_io_t *io, uint16_t code, uint32_t transaction, bool session_param, uint16_t *response);
ptp_data_status_t ptp_wire_request_data_result(const ptp_wire_io_t *io, uint16_t opcode,
    uint32_t transaction, const uint32_t *params, unsigned count, uint8_t *output,
    size_t capacity, size_t *size, uint16_t *response);
typedef int (*ptp_wire_packet_fn)(const ptp_wire_io_t *io, uint8_t *packet, size_t capacity);
bool ptp_wire_send_data(const ptp_wire_io_t *io, uint16_t opcode, uint32_t transaction,
    uint16_t property, const uint8_t *data, size_t size, uint16_t *response,
    ptp_wire_packet_fn receive);
int ptp_wire_initialization_exchange(const ptp_wire_io_t *io, uint8_t *packet,
    size_t request_length, size_t capacity, ptp_wire_packet_fn receive);
bool ptp_wire_decode_event(const uint8_t *packet, size_t size, ptpip_event_t *event);
