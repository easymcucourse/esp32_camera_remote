#pragma once
#include "camera_backend.h"
#include "camera_menu.h"
/* Owner-task copied snapshot. No borrowed descriptor survives read(). Backend
 * buffers remain caller-owned. Only a successful, fully validated snapshot may
 * be applied; failed/missing capabilities clear writable targets on apply. */
typedef struct {
    camera_choice_state_t mode;
    camera_menu_snapshot_t menu;
    camera_capabilities_t capabilities;
    camera_value_t values[CAMERA_SETTING_COUNT];
    bool seen[CAMERA_SETTING_COUNT], valid;
} camera_properties_t;
/* Single private semantic menu mapping shared by collection and execution. */
extern const camera_setting_t camera_menu_settings[CAMERA_MENU_COUNT];
camera_backend_result_t camera_properties_read(camera_backend_t *backend,
    void *scratch, size_t capacity, uint32_t timeout_ms, camera_properties_t *snapshot);
bool camera_properties_apply(const camera_properties_t *snapshot,
    setting_control_t *mode, camera_menu_t *menu, uint32_t now_ms);
