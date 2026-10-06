#pragma once
#include "camera_actions.h"
#include "camera_properties.h"
#include "app_message.h"

/* Backend/kernel calls stay on the Camera owner. Its message task may submit
 * controls/read caps using the optional paired guard below. Guards have static
 * lifetime, are set before session(), and never cover network/property I/O.
 * Domain generation and safety generation have different lifetimes. */
typedef struct {
    void (*enter)(void *context), (*leave)(void *context);
    void *guard_context;
    camera_actions_t actions;
    gamepad_caps_t caps;
    bool record_wait, record_target, record_executing;
    uint32_t record_deadline;
    setting_status_t status[APP_CAMERA_CONTROL_COUNT];
    uint32_t changed;
} camera_controls_t;
void camera_controls_session(camera_controls_t *controls, bool open, pad_lens_t lens);
bool camera_controls_submit(camera_controls_t *controls, pad_action_t action, uint32_t now);
void camera_controls_observe(camera_controls_t *controls, const camera_capabilities_t *caps, uint32_t now);
/* Execute at most one queued control. Record/zoom presses refresh properties
 * before eligibility and revalidate safety generation after that I/O. Releases
 * bypass those checks. OK means continue the session, including a rejected
 * press; rejected/failed release returns an error and retains its latch.
 * Caller closes the session on such an error. Scratch is never retained. */
/* NULL scratch/capacity0 selects direct controls only. It preserves deferred
 * record/zoom presses, but still executes releases while JPEG slots are leased. */
camera_backend_result_t camera_controls_tick(camera_controls_t *controls,
    camera_backend_t *backend, camera_properties_t *properties,
    setting_control_t *mode, camera_menu_t *menu, void *scratch, size_t capacity,
    uint32_t timeout_ms, uint32_t (*now_ms)(void *context), void *clock_context);
