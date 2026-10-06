#include "camera_backend.h"
#include <assert.h>
#include <string.h>
static unsigned owner;
static camera_backend_result_t connect_camera(void *context, const camera_connection_t *connection,
    void *scratch, size_t capacity, uint32_t timeout, camera_peer_t *peer)
{ assert(context == &owner && connection && scratch && capacity && timeout && peer); ++owner; return CAMERA_BACKEND_OK; }
static camera_backend_result_t disconnect_camera(void *context, uint32_t timeout)
{ assert(context == &owner && timeout); --owner; return CAMERA_BACKEND_OK; }
static camera_backend_result_t destroy_camera(void *context)
{ assert(context == &owner); return owner ? CAMERA_BACKEND_STATE : CAMERA_BACKEND_OK; }
static void cancel_camera(void *context) { assert(context == &owner); }
static void network_camera(void *context, uint32_t generation) { assert(context == &owner && generation); }
static camera_backend_result_t properties(void *c, void *b, size_t n, uint32_t t,
    camera_property_visitor_t v, void *u, camera_capabilities_t *caps)
{ (void)c; (void)b; (void)n; (void)t; (void)v; (void)u; (void)caps; return CAMERA_BACKEND_OK; }
static camera_backend_result_t set(void *c, camera_setting_t s, camera_value_t v, uint32_t t)
{ (void)c; (void)s; (void)v; (void)t; return CAMERA_BACKEND_OK; }
static camera_backend_result_t step(void *c, camera_setting_t s, int d, uint32_t t)
{ (void)c; (void)s; (void)d; (void)t; return CAMERA_BACKEND_OK; }
static camera_backend_result_t frame(void *c, void *b, size_t n, uint32_t t, camera_frame_t *f)
{ (void)c; (void)b; (void)n; (void)t; (void)f; return CAMERA_BACKEND_OK; }
static camera_backend_result_t action(void *c, camera_action_t a, int v, uint32_t t)
{ (void)c; (void)a; (void)v; (void)t; return CAMERA_BACKEND_OK; }
static camera_backend_result_t events(void *c, uint32_t t, bool *changed)
{ (void)c; (void)t; (void)changed; return CAMERA_BACKEND_OK; }
static const camera_backend_ops_t full = {
    .api_version = CAMERA_BACKEND_API_VERSION,
    .capabilities = CAMERA_BACKEND_CAP_PROPERTIES | CAMERA_BACKEND_CAP_ACTIONS |
        CAMERA_BACKEND_CAP_LIVEVIEW | CAMERA_BACKEND_CAP_EVENTS,
    .connect = connect_camera, .disconnect = disconnect_camera, .destroy = destroy_camera,
    .cancel = cancel_camera, .network_changed = network_camera, .properties = properties,
    .set = set, .step = step, .liveview = frame, .action = action, .events = events
};
static void reject(camera_backend_ops_t ops)
{
    camera_backend_t backend = {0}, zero = {0};
    assert(camera_backend_bind(&backend, &ops, &owner, 0) == CAMERA_BACKEND_INVALID);
    assert(!memcmp(&backend, &zero, sizeof backend));
}
int main(void)
{
    camera_backend_ops_t broken = full;
    broken.api_version++; reject(broken);
    broken = full; broken.capabilities |= 1u << 20; reject(broken);
    broken = full; broken.capabilities |= CAMERA_BACKEND_CAP_PROBE; reject(broken);
#define MISSING(op) do { broken = full; broken.op = NULL; reject(broken); } while (0)
    MISSING(connect); MISSING(disconnect); MISSING(cancel); MISSING(network_changed);
    MISSING(destroy); MISSING(properties); MISSING(set); MISSING(step);
    MISSING(liveview); MISSING(action); MISSING(events);
    camera_backend_t backend = {0};
    assert(camera_backend_bind(NULL, &full, &owner, 0) == CAMERA_BACKEND_INVALID);
    assert(camera_backend_bind(&backend, NULL, &owner, 0) == CAMERA_BACKEND_INVALID);
    assert(camera_backend_bind(&backend, &full, NULL, 0) == CAMERA_BACKEND_INVALID);
    assert(camera_backend_bind(&backend, &full, &owner, 1u << 20) == CAMERA_BACKEND_INVALID);
    static const camera_backend_ops_t minimal = {
        .api_version = CAMERA_BACKEND_API_VERSION,
        .connect = connect_camera, .disconnect = disconnect_camera, .cancel = cancel_camera,
        .network_changed = network_camera, .destroy = destroy_camera
    };
    assert(camera_backend_bind(&backend, &minimal, &owner, CAMERA_BACKEND_CAP_LIVEVIEW) == CAMERA_BACKEND_INVALID);
    assert(!backend.ops && !backend.context);
    assert(camera_backend_bind(&backend, &minimal, &owner, 0) == CAMERA_BACKEND_OK);
    camera_backend_t original = backend;
    assert(camera_backend_bind(&backend, &full, &owner, 0) == CAMERA_BACKEND_STATE);
    assert(!memcmp(&backend, &original, sizeof backend));
    camera_connection_t target = {.port = 15740}; camera_peer_t peer = {0};
    unsigned scratch = 0;
    assert(backend.ops->connect(backend.context, &target, &scratch, sizeof scratch, 1000, &peer) == CAMERA_BACKEND_OK);
    assert(backend.ops->destroy(backend.context) == CAMERA_BACKEND_STATE);
    backend.ops->cancel(backend.context); backend.ops->network_changed(backend.context, 2);
    assert(backend.ops->disconnect(backend.context, 1000) == CAMERA_BACKEND_OK);
    assert(backend.ops->destroy(backend.context) == CAMERA_BACKEND_OK);
    backend = (camera_backend_t){0};
    assert(camera_backend_bind(&backend, &full, &owner, full.capabilities) == CAMERA_BACKEND_OK);
    assert(backend.ops == &full && backend.api_version == 1 && backend.capabilities == full.capabilities);
    assert(camera_backend_validate(&backend, full.capabilities) == CAMERA_BACKEND_OK);
    backend.api_version++;
    assert(camera_backend_validate(&backend, 0) == CAMERA_BACKEND_INVALID);
    backend.api_version--;
    backend.capabilities = 0;
    assert(camera_backend_validate(&backend, 0) == CAMERA_BACKEND_INVALID);
    return 0;
}
