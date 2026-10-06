#pragma once
#include "camera_properties.h"
/* Owner task. One permit comes from one newly read/applied valid snapshot and
 * is consumed even if no target can be issued. Mode targets run first; while
 * Mode awaits actual readback, menu targets stay pending. Accepted writes do
 * not change actual values. Return OK on complete protocol rejection so a
 * healthy session can continue; return other failures to its cleanup owner. */
camera_backend_result_t camera_settings_execute(camera_backend_t *backend,
    const camera_properties_t *properties, setting_control_t *mode,
    camera_menu_t *menu, bool *fresh, uint32_t now_ms, uint32_t timeout_ms,
    bool *issued);
