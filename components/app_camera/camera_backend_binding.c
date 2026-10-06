#include "camera_session.h"
#include "camera_backend_sony_factory.h"
/* Private build selection; app_core never sees a backend or factory. Domain
 * owner validates the result before connect, retaining rejected objects for
 * cleanup. Runtime/session/properties include only the generic backend API. */
const camera_backend_factory_t camera_backend_default_factory = {
    .api_version = CAMERA_FACTORY_API_VERSION,
    .required_capabilities = CAMERA_BACKEND_CAP_PROPERTIES | CAMERA_BACKEND_CAP_LIVEVIEW |
        CAMERA_BACKEND_CAP_ACTIONS | CAMERA_BACKEND_CAP_EVENTS | CAMERA_BACKEND_CAP_PROBE,
    .create = camera_backend_sony_create
};
