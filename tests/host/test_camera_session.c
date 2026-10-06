#include "camera_session.h"
#include <assert.h>
#include <string.h>
typedef struct {
    camera_backend_t interface;
    bool alive;
    uint32_t generation;
    bool (*cancelled)(void *context);
    void *cancel_context;
} fake_t;
static fake_t fake;
static unsigned creates, connects, disconnects, destroys;
static camera_backend_result_t create_result, connect_result, disconnect_result, destroy_result;
static bool stopped, invalid_interface, owned_create_failure;
static camera_backend_result_t connect_camera(void *context, const camera_connection_t *connection,
    void *scratch, size_t capacity, uint32_t timeout, camera_peer_t *peer)
{
    assert(context == &fake && connection && scratch && capacity == 16 && timeout == 5000);
    assert(fake.alive && fake.cancelled && fake.cancel_context == &stopped); ++connects;
    memcpy(peer->model, "fake camera", 12);
    return connect_result;
}
static camera_backend_result_t disconnect_camera(void *context, uint32_t timeout)
{ assert(context == &fake && fake.alive && timeout == 1000); ++disconnects; return disconnect_result; }
static camera_backend_result_t destroy_camera(void *context)
{
    assert(context == &fake && fake.alive); ++destroys;
    if (destroy_result == CAMERA_BACKEND_OK) fake.alive = false;
    return destroy_result;
}
static void cancel_camera(void *context) { assert(context == &fake && fake.alive); }
static void network_camera(void *context, uint32_t generation) { assert(context == &fake && generation); }
static const camera_backend_ops_t ops = {.api_version = CAMERA_BACKEND_API_VERSION,
    .connect = connect_camera, .disconnect = disconnect_camera, .destroy = destroy_camera,
    .cancel = cancel_camera, .network_changed = network_camera};
static camera_backend_result_t create(uint32_t generation, bool (*cancelled)(void *),
    void *context, camera_backend_t **backend)
{
    assert(!fake.alive && generation && !*backend); ++creates;
    if (create_result != CAMERA_BACKEND_OK && !owned_create_failure) return create_result;
    fake = (fake_t){.alive = true, .generation = generation, .cancelled = cancelled, .cancel_context = context};
    assert(camera_backend_bind(&fake.interface, &ops, &fake, 0) == CAMERA_BACKEND_OK);
    if (invalid_interface) ++fake.interface.api_version;
    *backend = &fake.interface; return create_result;
}
static const camera_backend_factory_t factory = {.api_version = CAMERA_FACTORY_API_VERSION, .create = create};
static bool cancelled(void *context) { assert(context == &stopped); return stopped; }
static camera_session_t session;
static camera_connection_t connection = {.address = "fake", .port = 1};
static uint8_t scratch[16];
static camera_peer_t peer;
static camera_backend_result_t open(void)
{ return camera_session_open(&session, &connection, scratch, sizeof scratch, 5000, &peer); }
int main(void)
{
    camera_backend_factory_t invalid = factory; ++invalid.api_version;
    assert(camera_session_init(&session, &invalid, cancelled, &stopped) == CAMERA_BACKEND_INVALID);
    invalid = factory; invalid.required_capabilities = 1u << 20;
    assert(camera_session_init(&session, &invalid, cancelled, &stopped) == CAMERA_BACKEND_INVALID);
    assert(!session.factory && !session.backend && !creates);
    assert(camera_session_init(&session, &factory, cancelled, &stopped) == CAMERA_BACKEND_OK);
    assert(camera_session_init(&session, &factory, cancelled, &stopped) == CAMERA_BACKEND_STATE);
    stopped = true; assert(open() == CAMERA_BACKEND_CANCELLED && !creates); stopped = false;
    create_result = CAMERA_BACKEND_NO_MEMORY;
    assert(open() == CAMERA_BACKEND_NO_MEMORY && !session.backend && session.state == CAMERA_SESSION_EMPTY);
    uint32_t generation = session.generation; create_result = CAMERA_BACKEND_OK;
    connect_result = CAMERA_BACKEND_TIMEOUT; memset(&peer, 0, sizeof peer);
    assert(open() == CAMERA_BACKEND_TIMEOUT && session.state == CAMERA_SESSION_CLEANUP && session.backend);
    assert(!peer.model[0] && session.generation != generation && fake.generation == session.generation);
    assert(!camera_session_backend(&session)); unsigned created = creates;
    assert(open() == CAMERA_BACKEND_STATE && creates == created);
    disconnect_result = CAMERA_BACKEND_TIMEOUT;
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_TIMEOUT && session.backend && !destroys);
    disconnect_result = CAMERA_BACKEND_OK; destroy_result = CAMERA_BACKEND_STATE;
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_STATE && session.state == CAMERA_SESSION_DESTROY);
    unsigned disconnected = disconnects;
    destroy_result = CAMERA_BACKEND_OK;
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK && !session.backend && !fake.alive);
    assert(disconnects == disconnected && session.state == CAMERA_SESSION_EMPTY);
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK && disconnects == disconnected);
    generation = session.generation; connect_result = CAMERA_BACKEND_OK;
    assert(open() == CAMERA_BACKEND_OK && session.state == CAMERA_SESSION_CONNECTED);
    assert(camera_session_backend(&session) == &fake.interface && session.generation != generation);
    assert(camera_session_probe(&session, "fake", 1, 800) == CAMERA_BACKEND_STATE);
    assert(camera_session_backend(&session) == &fake.interface);
    assert(!strcmp(peer.model, "fake camera"));
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK);
    /* Version rejection occurs before connect but still owns the factory result. */
    invalid_interface = true; unsigned connected = connects;
    assert(open() == CAMERA_BACKEND_INVALID && connects == connected && session.backend);
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK); invalid_interface = false;
    /* Factory error after allocating a handle does not lose that ownership. */
    create_result = CAMERA_BACKEND_NO_MEMORY; owned_create_failure = true;
    assert(open() == CAMERA_BACKEND_NO_MEMORY && session.backend && connects == connected);
    assert(camera_session_close(&session, 1000) == CAMERA_BACKEND_OK);
    create_result = CAMERA_BACKEND_OK; owned_create_failure = false;
    camera_session_t required = {0}; invalid = factory;
    invalid.required_capabilities = CAMERA_BACKEND_CAP_PROPERTIES;
    assert(camera_session_init(&required, &invalid, cancelled, &stopped) == CAMERA_BACKEND_OK);
    assert(camera_session_open(&required, &connection, scratch, sizeof scratch, 5000, &peer) == CAMERA_BACKEND_INVALID);
    assert(connects == connected && camera_session_close(&required, 1000) == CAMERA_BACKEND_OK);
    assert(!fake.alive);
    return 0;
}
