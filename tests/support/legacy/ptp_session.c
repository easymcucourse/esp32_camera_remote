#include "ptp_codes.h"
#include "ptp_session.h"
#include "ptpip_packet.h"
#include "ptpip_transport.h"
#include "ptp_wire.h"
#include "lwip/sockets.h"
#include <errno.h>
#include "esp_log.h"
static const char *TAG = "camera_pair";
/* Temporary old producer path. The framing engine is shared with the client;
 * delete these fd wrappers when Camera/Sony switch to the instance API. */
static bool transfer(void *context, void *buffer, size_t size, bool transmit)
{ return ptpip_transfer(*(int *)context, buffer, size, transmit); }
static bool begin(void *context) { return ptpip_transaction_begin(*(int *)context); }
static void end(void *context) { (void)context; ptpip_transaction_end(); }
static ptp_wire_io_t legacy(int *fd) { return (ptp_wire_io_t){fd, transfer, begin, end}; }
int ptp_receive_packet(int fd, uint8_t *packet, size_t capacity)
{ ptp_wire_io_t io = legacy(&fd); return ptp_wire_receive_packet(&io, packet, capacity); }
bool ptp_operation(int fd, uint16_t code, uint32_t transaction, bool session_param)
{ ptp_wire_io_t io = legacy(&fd); return ptp_wire_operation(&io, code, transaction, session_param, NULL); }
ptp_data_status_t ptp_request_data_result(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned count, uint8_t *output, size_t capacity, size_t *size, uint16_t *response)
{ ptp_wire_io_t io = legacy(&fd); return ptp_wire_request_data_result(&io, opcode, transaction, params, count, output, capacity, size, response); }
bool ptp_request_data(int fd, uint16_t opcode, uint32_t transaction,
    const uint32_t *params, unsigned count, uint8_t *output, size_t capacity, size_t *size)
{ uint16_t response; return ptp_request_data_result(fd, opcode, transaction, params, count, output, capacity, size, &response) == PTP_DATA_OK; }

bool ptp_drain_events_changed(int event, bool *properties_changed)
{
    if (!properties_changed) return false;
    *properties_changed = false;
    // Drain events between frames so the independent event connection cannot fill.
    for (unsigned i = 0; i < 32; ++i) {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(event, &readable);
        struct timeval poll = {0};
        int ready = select(event + 1, &readable, NULL, NULL, &poll);
        if (ready == 0) return true;
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) return false;
        uint8_t packet[512];
        int length = ptp_receive_packet(event, packet, sizeof(packet));
        if (length < 8) return false;
        uint32_t type = get32(packet + 4);
        if (type == PTPIP_PROBE_REQUEST && length == 8) {
            put32(packet + 4, PTPIP_PROBE_RESPONSE);
            if (!ptpip_transfer(event, packet, 8, true)) return false;
        } else {
            ptpip_event_t event_value;
            if (!ptp_wire_decode_event(packet, (size_t)length, &event_value)) {
                ESP_LOGW(TAG, "Unexpected event type=%lu", (unsigned long)type); return false;
            }
            if (event_value.code == 0xc203) {
                // Captured param=0 identifies no specific property: refresh all.
                *properties_changed = true;
            }
        }
    }
    return true;
}


bool ptp_drain_events(int event)
{
    bool changed;
    return ptp_drain_events_changed(event, &changed);
}
