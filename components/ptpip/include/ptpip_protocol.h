#pragma once
#include "ptpip_client.h"
#include "ptp_result.h"
#include "ptp_event.h"
/* The client owns transaction allocation. The vendor supplies operation values,
 * never a second session/transaction counter. Standard packets use command. */
int ptpip_client_receive_packet(ptpip_client_t *client, ptpip_channel_kind_t kind,
    uint8_t *packet, size_t capacity);
bool ptpip_client_operation(ptpip_client_t *client, uint16_t opcode, bool session_param);
ptp_data_status_t ptpip_client_request_data(ptpip_client_t *client, uint16_t opcode,
    const uint32_t *params, unsigned count, uint8_t *output, size_t capacity,
    size_t *size, uint16_t *response);
/* Small control payload, <=116 bytes; vendor supplies wire values only. */
bool ptpip_client_send_data(ptpip_client_t *client, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted);
/* Tokens must already be open. Command acknowledgement owns connection/GUID/
 * name in this instance; backend applies its saved peer-identity policy.
 * Handshake timeout is explicit (current paired10s/unpaired120s). Success
 * preserves original first standard transaction2; event uses its channel's
 * timeout. Neither function registers endpoints or performs UI callbacks. */
bool ptpip_client_initialize_command(ptpip_client_t *client, const uint8_t guid[16],
    const char *name, unsigned timeout_ms);
bool ptpip_client_initialize_event(ptpip_client_t *client);
/* One packet per call; available=false means no bytes consumed. Vendor drains
 * at its existing bounded rate and interprets its own event codes. */
bool ptpip_client_next_event(ptpip_client_t *client, ptpip_event_t *event, bool *available);
