#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CAMERA_BACKEND_API_VERSION 1u
typedef enum {
    CAMERA_BACKEND_OK, CAMERA_BACKEND_INVALID, CAMERA_BACKEND_STATE,
    CAMERA_BACKEND_UNSUPPORTED, CAMERA_BACKEND_NO_MEMORY, CAMERA_BACKEND_CANCELLED,
    CAMERA_BACKEND_TIMEOUT, CAMERA_BACKEND_NETWORK, CAMERA_BACKEND_PROTOCOL,
    CAMERA_BACKEND_REFUSED, CAMERA_BACKEND_DROPPED, CAMERA_BACKEND_IDENTITY,
    CAMERA_BACKEND_NOT_READY /* Complete transient liveview refusal; safe retry. */
} camera_backend_result_t;
enum {
    CAMERA_BACKEND_CAP_PROPERTIES = 1u << 0,
    CAMERA_BACKEND_CAP_LIVEVIEW = 1u << 1,
    CAMERA_BACKEND_CAP_ACTIONS = 1u << 2,
    CAMERA_BACKEND_CAP_EVENTS = 1u << 3,
    CAMERA_BACKEND_CAP_PROBE = 1u << 4
};
/* Semantic IDs, never vendor property/operation codes. */
typedef enum {
    CAMERA_SETTING_MODE, CAMERA_SETTING_SHUTTER, CAMERA_SETTING_APERTURE,
    CAMERA_SETTING_ISO, CAMERA_SETTING_EV, CAMERA_SETTING_WB, CAMERA_SETTING_FOCUS,
    CAMERA_SETTING_METER, CAMERA_SETTING_ASPECT, CAMERA_SETTING_DRIVE,
    CAMERA_SETTING_EFFECT, CAMERA_SETTING_DRO, CAMERA_SETTING_AF_AREA,
    CAMERA_SETTING_WL_FLASH, CAMERA_SETTING_WB_TEMP, CAMERA_SETTING_WB_AB,
    CAMERA_SETTING_WB_GM, CAMERA_SETTING_BATTERY, CAMERA_SETTING_ZOOM_ENABLED,
    CAMERA_SETTING_RECORDING_STATE, CAMERA_SETTING_FLASH, CAMERA_SETTING_COUNT
} camera_setting_t;
typedef enum { CAMERA_VALUE_U8, CAMERA_VALUE_I8, CAMERA_VALUE_U16,
    CAMERA_VALUE_I16, CAMERA_VALUE_U32, CAMERA_VALUE_I32 } camera_value_type_t;
/* Signed values keep their bit pattern; no narrowing before type validation. */
typedef struct { camera_value_type_t type; uint32_t bits; } camera_value_t;
typedef struct {
    camera_setting_t setting;
    camera_value_t current;
    bool writable, relative;
    const camera_value_t *choices;
    size_t choice_count;
} camera_property_t;
typedef void (*camera_property_visitor_t)(void *context, const camera_property_t *property);
typedef struct {
    bool focus_known, manual_focus, zoom_known, zoom_enabled;
    bool recording_known, recording;
} camera_capabilities_t;
typedef struct { uint8_t guid[16]; char name[64], model[24], firmware[24]; } camera_peer_t;
typedef struct { const uint8_t *jpeg; size_t size; } camera_frame_t;
typedef enum { CAMERA_ACTION_FOCUS_STEP, CAMERA_ACTION_SHUTTER_HALF,
    CAMERA_ACTION_SHUTTER_FULL, CAMERA_ACTION_RECORD, CAMERA_ACTION_ZOOM } camera_action_t;
typedef struct {
    char address[16]; uint16_t port;
    uint8_t local_guid[16]; char local_name[64];
    bool require_peer_guid; uint8_t peer_guid[16]; /* Saved identity policy, checked before session I/O. */
    unsigned handshake_timeout_ms; /* Existing paired/unpaired policy supplied by app_camera. */
} camera_connection_t;
typedef struct camera_backend camera_backend_t;
typedef struct {
    unsigned api_version;
    uint32_t capabilities;
    camera_backend_result_t (*connect)(void *context, const camera_connection_t *connection,
        void *scratch, size_t capacity, uint32_t timeout_ms, camera_peer_t *peer);
    camera_backend_result_t (*disconnect)(void *context, uint32_t timeout_ms);
    void (*cancel)(void *context);
    void (*network_changed)(void *context, uint32_t generation);
    camera_backend_result_t (*destroy)(void *context);
    camera_backend_result_t (*properties)(void *context, void *scratch, size_t capacity,
        uint32_t timeout_ms, camera_property_visitor_t visitor, void *visitor_context,
        camera_capabilities_t *capabilities);
    camera_backend_result_t (*set)(void *context, camera_setting_t setting,
        camera_value_t value, uint32_t timeout_ms);
    camera_backend_result_t (*step)(void *context, camera_setting_t setting,
        int direction, uint32_t timeout_ms);
    camera_backend_result_t (*liveview)(void *context, void *scratch, size_t capacity,
        uint32_t timeout_ms, camera_frame_t *frame);
    camera_backend_result_t (*action)(void *context, camera_action_t action,
        int value, uint32_t timeout_ms);
    camera_backend_result_t (*events)(void *context, uint32_t timeout_ms, bool *properties_changed);
    /* TCP reachability only: no handshake/session/transaction/identity write.
     * Success or failed cleanup can own a channel until disconnect succeeds. */
    camera_backend_result_t (*probe)(void *context, const char *address, uint16_t port, uint32_t timeout_ms);
} camera_backend_ops_t;
/* Embedded by exactly one implementation; app_camera alone owns this handle.
 * All calls synchronous on its owner task, except cancel/network_changed which
 * must be task-safe and nonblocking. Positive timeouts bound a whole operation,
 * including multiple protocol requests; cleanup failure retains ownership for
 * retry. destroy requires stopped I/O and no owned channels; success invalidates
 * the complete backend handle. No operation revokes buffers before returning.
 * Properties are fully validated before callbacks; descriptors/choice arrays
 * are borrowed only during callbacks. Frame bytes borrow caller scratch and
 * remain valid until it is reused. On failed reads no snapshot/frame is emitted.
 * OK on set/action means protocol acceptance, not observed physical completion.
 * Implementation diagnostics remain private; callers use only these results.
 */
struct camera_backend {
    unsigned api_version;
    uint32_t capabilities;
    void *context;
    const camera_backend_ops_t *ops;
};
/* Bind fresh zeroed embedded storage once, after validating all advertised ops.
 * No ownership transfer on failure. No allocation or protocol/backend dependency.
 * Ops have static lifetime; caller checks required capabilities at construction.
 */
camera_backend_result_t camera_backend_bind(camera_backend_t *backend,
    const camera_backend_ops_t *ops, void *context, uint32_t required_capabilities);
/* Check a factory result before any I/O. Checks both interface and immutable
 * ops version/capability agreement, including every advertised operation. */
camera_backend_result_t camera_backend_validate(const camera_backend_t *backend,
    uint32_t required_capabilities);
