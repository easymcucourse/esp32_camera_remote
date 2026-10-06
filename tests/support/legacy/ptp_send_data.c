#include "ptp_session.h"
#include "ptpip_transport.h"
#include "ptp_codes.h"
#include "ptp_wire.h"
/* Temporary fd adapter; standard data-phase encoding lives only in ptp_wire. */
static bool transfer(void *context, void *buffer, size_t size, bool transmit)
{ return ptpip_transfer(*(int *)context, buffer, size, transmit); }
static bool begin(void *context) { return ptpip_transaction_begin(*(int *)context); }
static void end(void *context) { (void)context; ptpip_transaction_end(); }
static int packet(const ptp_wire_io_t *io, uint8_t *buffer, size_t size)
{ return ptp_receive_packet(*(int *)io->context, buffer, size); }
int ptp_initialization_exchange(int fd, uint8_t *buffer, size_t request_length, size_t capacity)
{
    ptp_wire_io_t io = {&fd, transfer, begin, end};
    return ptp_wire_initialization_exchange(&io, buffer, request_length, capacity, packet);
}
bool ptp_send_data(int fd, uint32_t transaction, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted)
{
    if (!accepted) return false;
    *accepted = false;
    ptp_wire_io_t io = {&fd, transfer, begin, end}; uint16_t response = 0;
    bool result = ptp_wire_send_data(&io, opcode, transaction, property, data, size, &response, packet);
    *accepted = result && response == PTP_RC_OK; return result;
}
