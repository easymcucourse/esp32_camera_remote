#pragma once
#include "camera_session.h"
#include "app_message.h"
typedef struct {
    app_network_client_t peer;
    int selected; /* Original -1 none, -2 multiple, otherwise DHCP snapshot index. */
    bool connect_failed;
} camera_discovery_t;
/* Camera owner task, registered endpoint and nonzero message domain generation.
 * Uses DHCP snapshot message only, never scans IP ranges. Probe instances are
 * sequential and explicitly closed/destroyed before the next candidate, so the
 * two backend channel lanes cannot be exhausted by concurrent discovery fds.
 * Cleanup failure preserves session ownership and aborts scan; caller retries
 * cleanup before another scan. Success selects exactly one reachable peer via
 * WIFI_SELECT_CAMERA message; multiple/no peer publish no partial target. */
camera_backend_result_t camera_discovery_scan(camera_session_t *probe,
    uint32_t generation, bool paired, const uint8_t saved_mac[6], camera_discovery_t *result);
camera_backend_result_t camera_network_select(uint32_t generation, const uint8_t mac[6]);
