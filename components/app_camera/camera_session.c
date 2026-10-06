#include "camera_session.h"
#include <stdatomic.h>

static atomic_uint generations;
uint32_t camera_session_next_generation(void)
{
    uint32_t generation = atomic_fetch_add(&generations, 1) + 1;
    if (!generation) generation = atomic_fetch_add(&generations, 1) + 1;
    return generation;
}
camera_backend_result_t camera_session_init(camera_session_t *session,
    const camera_backend_factory_t *factory, bool (*cancelled)(void *context), void *context)
{
    const uint32_t known = CAMERA_BACKEND_CAP_PROPERTIES | CAMERA_BACKEND_CAP_LIVEVIEW |
        CAMERA_BACKEND_CAP_ACTIONS | CAMERA_BACKEND_CAP_EVENTS | CAMERA_BACKEND_CAP_PROBE;
    if (!session || !factory || factory->api_version != CAMERA_FACTORY_API_VERSION || !factory->create ||
        (factory->required_capabilities & ~known))
        return CAMERA_BACKEND_INVALID;
    if (session->factory || session->backend || session->generation || session->state != CAMERA_SESSION_EMPTY)
        return CAMERA_BACKEND_STATE;
    *session = (camera_session_t){.factory = factory, .cancelled = cancelled, .cancel_context = context};
    return CAMERA_BACKEND_OK;
}
static camera_backend_result_t create(camera_session_t *s, uint32_t additional_capabilities)
{
    if (!s || !s->factory)
        return CAMERA_BACKEND_INVALID;
    if (s->backend || s->state != CAMERA_SESSION_EMPTY) return CAMERA_BACKEND_STATE;
    if (s->cancelled && s->cancelled(s->cancel_context)) return CAMERA_BACKEND_CANCELLED;
    s->generation = camera_session_next_generation();
    s->state = CAMERA_SESSION_CONNECTING;
    camera_backend_result_t result = s->factory->create(s->generation, s->cancelled,
        s->cancel_context, &s->backend);
    if (result == CAMERA_BACKEND_OK)
        result = camera_backend_validate(s->backend, s->factory->required_capabilities | additional_capabilities);
    if (result != CAMERA_BACKEND_OK)
        s->state = s->backend ? CAMERA_SESSION_CLEANUP : CAMERA_SESSION_EMPTY;
    return result;
}
camera_backend_result_t camera_session_open(camera_session_t *s,
    const camera_connection_t *connection, void *scratch, size_t capacity,
    uint32_t timeout, camera_peer_t *peer)
{
    if (!connection || !scratch || !capacity || !timeout || !peer) return CAMERA_BACKEND_INVALID;
    camera_backend_result_t result = create(s, 0);
    if (result != CAMERA_BACKEND_OK) return result;
    camera_peer_t found = {0};
    if (result == CAMERA_BACKEND_OK) result = s->backend->ops->connect(s->backend->context,
        connection, scratch, capacity, timeout, &found);
    if (result == CAMERA_BACKEND_OK) *peer = found;
    s->state = result == CAMERA_BACKEND_OK ? CAMERA_SESSION_CONNECTED :
        s->backend ? CAMERA_SESSION_CLEANUP : CAMERA_SESSION_EMPTY;
    return result;
}
camera_backend_result_t camera_session_probe(camera_session_t *s, const char *address,
    uint16_t port, uint32_t timeout)
{
    if (!address || !*address || !port || !timeout) return CAMERA_BACKEND_INVALID;
    camera_backend_result_t result = create(s, CAMERA_BACKEND_CAP_PROBE);
    if (result != CAMERA_BACKEND_OK) return result;
    result = s->backend->ops->probe(s->backend->context, address, port, timeout);
    s->state = CAMERA_SESSION_CLEANUP;
    return result;
}
camera_backend_result_t camera_session_close(camera_session_t *s, uint32_t timeout)
{
    if (!s || !s->factory || !timeout) return CAMERA_BACKEND_INVALID;
    if (!s->backend) { s->state = CAMERA_SESSION_EMPTY; return CAMERA_BACKEND_OK; }
    bool destroy_only = s->state == CAMERA_SESSION_DESTROY;
    s->state = destroy_only ? CAMERA_SESSION_DESTROY : CAMERA_SESSION_CLEANUP;
    /* Even a rejected factory interface keeps its ownership. Cleanup only
     * calls a current-version ops table with mandatory cleanup callbacks. */
    const camera_backend_ops_t *ops = s->backend->ops;
    if (!ops || ops->api_version != CAMERA_BACKEND_API_VERSION || !s->backend->context ||
        !ops->disconnect || !ops->destroy) return CAMERA_BACKEND_INVALID;
    camera_backend_result_t result = destroy_only ? CAMERA_BACKEND_OK : ops->disconnect(s->backend->context, timeout);
    if (result == CAMERA_BACKEND_OK) {
        s->state = CAMERA_SESSION_DESTROY;
        result = ops->destroy(s->backend->context);
    }
    if (result == CAMERA_BACKEND_OK) { s->backend = NULL; s->state = CAMERA_SESSION_EMPTY; }
    return result;
}
camera_backend_t *camera_session_backend(camera_session_t *s)
{ return s && s->state == CAMERA_SESSION_CONNECTED ? s->backend : NULL; }
