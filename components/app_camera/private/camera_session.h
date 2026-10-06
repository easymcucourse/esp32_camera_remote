#pragma once
#include "camera_backend.h"
#include "camera_generation.h"

#define CAMERA_FACTORY_API_VERSION 1u
typedef struct {
    unsigned api_version;
    uint32_t required_capabilities;
    camera_backend_result_t (*create)(uint32_t generation,
        bool (*cancelled)(void *context), void *context, camera_backend_t **backend);
} camera_backend_factory_t;
extern const camera_backend_factory_t camera_backend_default_factory;
typedef enum { CAMERA_SESSION_EMPTY, CAMERA_SESSION_CONNECTING,
    CAMERA_SESSION_CONNECTED, CAMERA_SESSION_CLEANUP, CAMERA_SESSION_DESTROY } camera_session_state_t;
typedef struct {
    const camera_backend_factory_t *factory;
    camera_backend_t *backend;
    bool (*cancelled)(void *context);
    void *cancel_context;
    uint32_t generation;
    camera_session_state_t state;
} camera_session_t;
/* All calls on Camera's sole owner task. Factory/static ops/predicate context
 * live through close; cross-task stop is expressed only by the task-safe
 * predicate, never by racing a borrowed backend pointer with destroy. Domain
 * phase tracks ownership/cleanup only, not PTP's session-open or transactions.
 * Each created lifetime has a fresh domain generation. Failed create/connect,
 * rejected interface, disconnect or destroy retains the handle until explicit
 * cleanup succeeds. No reconnect may overwrite that ownership. */
camera_backend_result_t camera_session_init(camera_session_t *session,
    const camera_backend_factory_t *factory, bool (*cancelled)(void *context), void *context);
camera_backend_result_t camera_session_open(camera_session_t *session,
    const camera_connection_t *connection, void *scratch, size_t capacity,
    uint32_t timeout_ms, camera_peer_t *peer);
camera_backend_result_t camera_session_close(camera_session_t *session, uint32_t timeout_ms);
camera_backend_result_t camera_session_probe(camera_session_t *session,
    const char *address, uint16_t port, uint32_t timeout_ms);
/* Borrowed only on owner task while CONNECTED; never retained by another task.
 * Normal operations call the validated ops directly: no forwarding wrappers. */
camera_backend_t *camera_session_backend(camera_session_t *session);
