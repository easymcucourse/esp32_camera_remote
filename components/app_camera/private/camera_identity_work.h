#pragma once
#include "camera_identity.h"
typedef enum { CAMERA_IDENTITY_LOAD, CAMERA_IDENTITY_CONFIRM, CAMERA_IDENTITY_FORGET } camera_identity_operation_t;
/* Exclusive Camera lifecycle operation. Owns an internal-RAM NVS worker and
 * waits for completion; caller keeps identity alive and excludes other users.
 * The worker copies identity/peer into its internal stack before NVS access,
 * and touches the caller/context only with caches enabled, after NVS returns.
 * Confirmation is allowed only after complete backend initialization. */
bool camera_identity_run(camera_identity_t *identity, camera_identity_operation_t operation,
    const uint8_t mac[6], const uint8_t peer_guid[16]);
