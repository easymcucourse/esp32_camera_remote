#include "camera_backend.h"

static bool valid_ops(const camera_backend_ops_t *ops, uint32_t required)
{
    const uint32_t known = CAMERA_BACKEND_CAP_PROPERTIES | CAMERA_BACKEND_CAP_LIVEVIEW |
        CAMERA_BACKEND_CAP_ACTIONS | CAMERA_BACKEND_CAP_EVENTS | CAMERA_BACKEND_CAP_PROBE;
    if (!ops || ops->api_version != CAMERA_BACKEND_API_VERSION ||
        (required & ~known) || (ops->capabilities & ~known) ||
        (ops->capabilities & required) != required || !ops->connect || !ops->disconnect ||
        !ops->cancel || !ops->network_changed || !ops->destroy ||
        ((ops->capabilities & CAMERA_BACKEND_CAP_PROPERTIES) &&
            (!ops->properties || !ops->set || !ops->step)) ||
        ((ops->capabilities & CAMERA_BACKEND_CAP_LIVEVIEW) && !ops->liveview) ||
        ((ops->capabilities & CAMERA_BACKEND_CAP_ACTIONS) && !ops->action) ||
        ((ops->capabilities & CAMERA_BACKEND_CAP_EVENTS) && !ops->events) ||
        ((ops->capabilities & CAMERA_BACKEND_CAP_PROBE) && !ops->probe)) return false;
    return true;
}
camera_backend_result_t camera_backend_validate(const camera_backend_t *backend, uint32_t required)
{
    if (!backend || !backend->context || !valid_ops(backend->ops, required) ||
        backend->api_version != backend->ops->api_version ||
        backend->capabilities != backend->ops->capabilities) return CAMERA_BACKEND_INVALID;
    return CAMERA_BACKEND_OK;
}
camera_backend_result_t camera_backend_bind(camera_backend_t *backend,
    const camera_backend_ops_t *ops, void *context, uint32_t required)
{
    if (!backend || !context || !valid_ops(ops, required)) return CAMERA_BACKEND_INVALID;
    if (backend->ops || backend->context || backend->api_version || backend->capabilities)
        return CAMERA_BACKEND_STATE;
    *backend = (camera_backend_t){ops->api_version, ops->capabilities, context, ops};
    return CAMERA_BACKEND_OK;
}
