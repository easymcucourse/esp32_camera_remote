#pragma once
#include "atom_protocol.h"

typedef struct {
    uint32_t boot_id, ack_id, local_mask;
    uint8_t seq, failures;
    bool online, mismatch;
} atom_client_t;
typedef enum { ATOM_CLIENT_RETRY, ATOM_CLIENT_OFFLINE, ATOM_CLIENT_MISMATCH,
    ATOM_CLIENT_HELLO, ATOM_CLIENT_POLL, ATOM_CLIENT_RESTART } atom_client_result_t;
atom_request_t atom_client_request(const atom_client_t *client);
atom_client_result_t atom_client_failure(atom_client_t *client);
/* On POLL, returns a borrowed pointer to a validated 28-byte payload. */
atom_client_result_t atom_client_response(atom_client_t *client, const uint8_t *bytes,
                                          size_t size, const uint8_t **payload);
