#pragma once
#include "camera_backend.h"
/* Private construction by app_camera; a fresh nonzero Camera domain generation
 * per backend lifetime. Predicate/context live until destroy returns OK. */
camera_backend_result_t camera_backend_sony_create(uint32_t generation,
    bool (*cancelled)(void *context), void *context, camera_backend_t **backend);
