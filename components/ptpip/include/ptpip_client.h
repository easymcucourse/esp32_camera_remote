#pragma once
#include "app_message.h"
#include <stdatomic.h>
#define PTPIP_CLIENT_API_VERSION 1
typedef enum { PTPIP_CHANNEL_COMMAND, PTPIP_CHANNEL_EVENT, PTPIP_CHANNEL_COUNT } ptpip_channel_kind_t;
typedef enum { PTPIP_CLIENT_OK, PTPIP_CLIENT_CANCELLED, PTPIP_CLIENT_TIMEOUT,
    PTPIP_CLIENT_NETWORK, PTPIP_CLIENT_PROTOCOL, PTPIP_CLIENT_INVALID,
    PTPIP_CLIENT_INIT_REJECTED } ptpip_client_result_t;
typedef struct {
    uint32_t token, generation;
    unsigned timeout_ms;
} ptpip_client_channel_t;
/* Embedded by a vendor backend. One owner task performs all protocol calls;
 * cancel/network_changed only update atomics and may run from another task.
 * No fd, driver pointer or second session/transaction owner is exposed. */
typedef struct {
    unsigned api_version;
    app_endpoint_t endpoint;
    uint32_t generation;
    ptpip_client_channel_t channels[PTPIP_CHANNEL_COUNT];
    atomic_bool cancelled;
    atomic_uint network_generation;
    bool (*cancel_predicate)(void *context);
    void *cancel_context;
    unsigned transaction_depth;
    int64_t transaction_deadline;
    int64_t transaction_parents[8]; /* Nested wire requests restore composite bound. */
    uint32_t session, next_transaction;
    uint16_t response; /* Implementation protocol diagnostic. */
    uint32_t connection_id, initialization_reason;
    uint8_t peer_guid[16];
    char peer_name[64];
    bool command_initialized, event_initialized;
    ptpip_client_result_t last_result;
    esp_err_t diagnostic;
    size_t transferred;
} ptpip_client_t;
/* Init fresh storage, or a stopped instance after all channel tokens are closed.
 * It never disposes a live object's ownership. Generation is a nonzero domain
 * lifetime value; reusing storage for a new session needs a fresh generation. */
bool ptpip_client_init(ptpip_client_t *client, app_endpoint_t endpoint, uint32_t generation,
    bool (*cancelled)(void *context), void *context);
void ptpip_client_cancel(ptpip_client_t *client);
void ptpip_client_network_changed(ptpip_client_t *client, uint32_t generation);
bool ptpip_client_open(ptpip_client_t *client, ptpip_channel_kind_t kind,
    const char *address, uint16_t port, unsigned timeout_ms);
/* Failure retains tokens for retry; success clears them. No consumer buffer
 * is revoked: transfer returns only after its last lease reference is back,
 * even when the router request times out or is cancelled first. */
bool ptpip_client_close(ptpip_client_t *client, ptpip_channel_kind_t kind, unsigned timeout_ms);
bool ptpip_client_transfer(ptpip_client_t *client, ptpip_channel_kind_t kind,
    void *buffer, size_t size, bool transmit);
bool ptpip_client_timeout_set(ptpip_client_t *client, ptpip_channel_kind_t kind, unsigned timeout_ms);
bool ptpip_client_poll(ptpip_client_t *client, ptpip_channel_kind_t kind, bool *readable);
bool ptpip_client_transaction_begin(ptpip_client_t *client, ptpip_channel_kind_t kind);
/* Vendor composite operation: begin once before OPEN/handshake/multiple PTP
 * requests, then transaction_end. All nested requests share the same absolute
 * deadline. Requires no existing scope; cleanup after expiry gets its own scope.
 */
bool ptpip_client_scope_begin(ptpip_client_t *client, unsigned timeout_ms);
/* Cleanup remains possible after cancel/network changes, sharing one bound. */
bool ptpip_client_cleanup_begin(ptpip_client_t *client, unsigned timeout_ms);
void ptpip_client_transaction_end(ptpip_client_t *client);
