#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "pad_types.h"
#include "pad_cmd.h"
#include "i2c_monitor.h"

#define APP_MESSAGE_API_VERSION 1
#define APP_MESSAGE_LEASE_CAPACITY 32

/* Envelope rules shared by every operation below (transport details in
 * app_console.h). APIs are task-safe, not ISR APIs; handlers run on their
 * endpoint owner, never as callbacks inside router locks. REQUEST uses an
 * absolute esp_timer deadline_us > 0 and a router-reserved correlation. REPLY
 * keeps type/correlation/generation/deadline from that reservation. Transport
 * ESP_OK only means a reply arrived: inspect reply.result for operation status.
 * Expiry cancels the wait, not work already executing or borrowed buffers.
 *
 * generation is mandatory and domain-specific: ordinary requests use source
 * endpoint lifetime; Camera discovery/PTP/session requests and publications use
 * their attempt/backend/producer lifetime. Wi-Fi
 * publications use network generation, Input publications use Input lifetime.
 * payload safety/report/channel generations remain separate. endpoint_epoch is
 * router-owned and protects the destination lifetime; callers leave it zero.
 *
 * Inline payload is copied with the message. BULK requires a lease; producer
 * retains buffer/context storage until its final callback, even after timeout
 * or partial fanout failure. Send consumes the supplied lease on every result;
 * a successful receive/reply gives one reference that its caller must release.
 * Queued leases are drained on endpoint stop, delivered leases are not revoked.
 * A lease-free control operation has an independent queue ahead of bulk.
 * EVENT delivery may fail; its producer defines retry/drop policy. Unsupported
 * IDs return NOT_SUPPORTED; malformed fields INVALID_ARG, closed/stale owner
 * INVALID_STATE, capacity/deadline failures NO_MEM/TIMEOUT as documented by
 * each handler. These codes do not promise rollback of already admitted work. */

typedef enum {
    APP_ENDPOINT_NONE, APP_ENDPOINT_SYSTEM, APP_ENDPOINT_CAMERA,
    APP_ENDPOINT_WIFI, APP_ENDPOINT_UI, APP_ENDPOINT_INPUT,
    APP_ENDPOINT_INPUT_SIM, APP_ENDPOINT_INPUT_ATOM, APP_ENDPOINT_UART,
    APP_ENDPOINT_COUNT
} app_endpoint_t;

/* Domain contracts carry semantic values; no driver / backend / socket pointers.
 * Requests/replies use the same operation type and correlation/generation;
 * flags distinguish replies. Events are routed only to frozen subscriptions. */
typedef enum {
    APP_MESSAGE_CAMERA_DISCOVER, APP_MESSAGE_WIFI_RSSI,
    APP_MESSAGE_WIFI_CHANNEL_OPEN, APP_MESSAGE_WIFI_CHANNEL_SEND,
    APP_MESSAGE_WIFI_CHANNEL_RECEIVE, APP_MESSAGE_WIFI_CHANNEL_CLOSE,
    APP_MESSAGE_WIFI_CONFIG_GET, APP_MESSAGE_WIFI_CONFIG_PREPARE,
    APP_MESSAGE_WIFI_CONFIG_COMMIT, APP_MESSAGE_WIFI_CONFIG_CANCEL,
    APP_MESSAGE_WIFI_CONFIG_RESULT, APP_MESSAGE_WIFI_SELECT_CAMERA,
    APP_MESSAGE_WIFI_NETWORK_CHANGED, APP_MESSAGE_WIFI_STATUS,
    APP_MESSAGE_CAMERA_START, APP_MESSAGE_CAMERA_STOP, APP_MESSAGE_CAMERA_FORGET, APP_MESSAGE_CAMERA_DISPLAY_SESSION,
    APP_MESSAGE_CAMERA_ACTION, APP_MESSAGE_CAMERA_SETTING_ADJUST,
    APP_MESSAGE_CAMERA_MENU_ACTION, APP_MESSAGE_CAMERA_FRAME, APP_MESSAGE_UI_FRAME_RESULT,
    APP_MESSAGE_CAMERA_STATE, APP_MESSAGE_CAMERA_CAPABILITIES,
    APP_MESSAGE_CAMERA_PROPERTIES, APP_MESSAGE_CAMERA_COMMAND_STATUS,
    APP_MESSAGE_CAMERA_STATUS, APP_MESSAGE_UI_MENU_ACTION,
    APP_MESSAGE_UI_STATE, APP_MESSAGE_UI_STATUS, APP_MESSAGE_UI_PREFERENCES, APP_MESSAGE_UI_PROPERTY_STATUS,
    APP_MESSAGE_INPUT_STATE, APP_MESSAGE_INPUT_STATUS, APP_MESSAGE_INPUT_SELECT,
    APP_MESSAGE_INPUT_SIM_COMMAND, APP_MESSAGE_INPUT_ATOM_COMMAND,
    APP_MESSAGE_DISPLAY_BENCH, APP_MESSAGE_DISPLAY_FAULT,
    APP_MESSAGE_SYSTEM_STATUS, APP_MESSAGE_SYSTEM_RESTART,
    /* Retired normal factory operations; retained values for message ABI.
     * System rejects these. Persistent reset is maintenance Web only. */
    APP_MESSAGE_SYSTEM_FACTORY_RESET, APP_MESSAGE_SYSTEM_FACTORY_RESULT,
    APP_MESSAGE_SYSTEM_CAMERA_SESSION, /* Transitional Core-owned normal maintenance policy. */
    APP_MESSAGE_SYSTEM_ENTER_NORMAL, /* UI admission before LIVE/settings/test pixels. */
    APP_MESSAGE_COUNT
} app_message_type_t;

/* CAMERA_START command.flag selects pairing diagnostics (false=preview).
 * CAMERA_STOP UART REQUEST may set command.flag for admission-only ACK,
 * preserving the nonblocking serial stop command. Core leaves it false to
 * await physical owner/lease drain. No other source may request admission ACK.
 * CAMERA_FORGET is retired and rejected; reset uses maintenance storage. */
/* Debug CAMERA_DISPLAY_SESSION is UI-only REQUEST: command.index=acquire1/
 * release0, token identifies one owned reservation. Acquire blocks starts,
 * drains the real owner, then replies token/flag=previously running. Release
 * cancels even an unacknowledged acquire; flag resumes preview after UI cleanup.
 * No buffer/task/backend handle crosses the message. */

enum {
    APP_MESSAGE_REQUEST = 1u << 0,
    APP_MESSAGE_REPLY = 1u << 1,
    APP_MESSAGE_EVENT = 1u << 2,
    APP_MESSAGE_BULK = 1u << 3,
};
typedef enum {
    APP_CAMERA_PROPERTY_MODE, APP_CAMERA_PROPERTY_SHUTTER,
    APP_CAMERA_PROPERTY_APERTURE, APP_CAMERA_PROPERTY_ISO,
    APP_CAMERA_PROPERTY_EV, APP_CAMERA_PROPERTY_WB,
    APP_CAMERA_PROPERTY_FOCUS, APP_CAMERA_PROPERTY_METER,
    APP_CAMERA_PROPERTY_FLASH, APP_CAMERA_PROPERTY_ASPECT,
    APP_CAMERA_PROPERTY_DRIVE, APP_CAMERA_PROPERTY_EFFECT,
    APP_CAMERA_PROPERTY_DRO, APP_CAMERA_PROPERTY_AF_AREA,
    APP_CAMERA_PROPERTY_WL_FLASH, APP_CAMERA_PROPERTY_WB_TEMP,
    APP_CAMERA_PROPERTY_WB_AB, APP_CAMERA_PROPERTY_WB_GM,
    APP_CAMERA_PROPERTY_BATTERY, APP_CAMERA_PROPERTY_COUNT
} app_camera_property_t;

typedef struct { uint8_t mac[6]; char ip[16]; int32_t rssi; } app_network_client_t;
typedef struct {
    char ssid[33], password[65];
    uint8_t channel;
    bool show_password, default_password;
} app_network_config_t;
typedef struct {
    app_network_config_t config;
    char address[16];
    uint32_t generation;
    unsigned max_channel;
    bool online;
} app_network_state_t;
typedef enum {
    APP_NETWORK_IO_OK, APP_NETWORK_IO_TIMEOUT, APP_NETWORK_IO_CANCELLED,
    APP_NETWORK_IO_STALE, APP_NETWORK_IO_CLOSED, APP_NETWORK_IO_INVALID,
    APP_NETWORK_IO_NO_MEMORY, APP_NETWORK_IO_FAILED
} app_network_io_status_t;
typedef enum { APP_CAMERA_STAGE_UNKNOWN, APP_CAMERA_STAGE_DISCOVERING,
    APP_CAMERA_STAGE_CONNECTING, APP_CAMERA_STAGE_INITIALIZING, APP_CAMERA_STAGE_LIVE,
    APP_CAMERA_STAGE_STOPPED, APP_CAMERA_STAGE_FAILED } app_camera_stage_t;
typedef struct {
    bool busy, session, stopped;
    int32_t last_io;
    app_camera_stage_t stage;
    char phase[96], model[24], firmware[24];
} app_camera_status_t;
typedef struct {
    uint32_t actual, target;
    unsigned status;
    bool writable, target_valid, actual_valid;
    app_camera_property_t property;
} app_camera_property_state_t;
/* UI_PROPERTY_STATUS UART REQUEST reads one displayed semantic property:
 * command.index=ASPECT..WB_GM, payload.property returns copied actual/target
 * and command status. Vendor property codes never cross this contract. */
/* Immutable bulk lease for one complete model update. No backend/vendor IDs or
 * borrowed enum pointers; index equals the semantic property ID. Producer
 * owns storage until the last message reference returns. */
typedef struct {
    app_camera_property_state_t properties[APP_CAMERA_PROPERTY_COUNT];
    gamepad_caps_t capabilities;
} app_camera_view_t;
/* CAMERA_ACTION replies include the post-admission capabilities (including
 * safety generation and record_pending), even on a live-owner rejection.
 * A nonzero capabilities.generation identifies a produced snapshot; an early
 * envelope/deadline error has no snapshot and cannot acknowledge safe release.
 * Successful admission is separate from Core's physical CAMERA_STOP drain. */
typedef enum { APP_CAMERA_CONTROL_SHUTTER_HALF, APP_CAMERA_CONTROL_SHUTTER_FULL,
    APP_CAMERA_CONTROL_RECORD, APP_CAMERA_CONTROL_ZOOM, APP_CAMERA_CONTROL_FOCUS_STEP,
    APP_CAMERA_CONTROL_COUNT } app_camera_control_t;
typedef struct {
    bool connected, atom_online, mismatch, sim;
    uint32_t buttons, source_epoch, report_id;
    int16_t rx, ry;
    uint8_t lt, rt, battery, kind, gimbal;
} app_input_state_t;
enum { APP_INPUT_PROVIDER_PAD_KIND, APP_INPUT_ATOM_LOG_MODE,
    APP_INPUT_ATOM_STATS, APP_INPUT_ATOM_LOG_READ };
/* ATOM owns its bounded protocol monitor. UART-only REQUEST operations select
 * log mode, read/reset stats (command.flag), or drain one copied frame. No
 * UART callback or borrowed monitor state is retained by the provider. */
typedef enum { APP_INPUT_SIM_PAD_KIND, APP_INPUT_SIM_ENABLE, APP_INPUT_SIM_ONLINE,
    APP_INPUT_SIM_REBOOT, APP_INPUT_SIM_VERSION, APP_INPUT_SIM_FAIL, APP_INPUT_SIM_CRC,
    APP_INPUT_SIM_TIMEOUT, APP_INPUT_SIM_GIMBAL, APP_INPUT_SIM_CONNECT,
    APP_INPUT_SIM_BATTERY, APP_INPUT_SIM_GAP, APP_INPUT_SIM_OVERFLOW,
    APP_INPUT_SIM_SEQUENCE, APP_INPUT_SIM_STATUS } app_input_sim_op_t;
/* SIM_COMMAND is a Debug provider command, never a Camera/UI action. UART
 * controls copied scalar values; SEQUENCE requires a readonly BULK lease of
 * exactly sizeof(pad_sequence_t), copied into the four-job player on admission.
 * command.token is caller-assigned. Completion is an EVENT with token,
 * duration_ms and flag=cancelled, retained if UART admission is congested.
 * Input alone sends PAD_KIND. STATUS returns flag=enabled/value=queued jobs;
 * UI may read STATUS to gate its Debug benchmark, but cannot change SIM. */
/* INPUT_SELECT command.index is source kind (ATOM0/UART_SIM1), UART/System
 * only. INPUT_ATOM_COMMAND index=PAD_KIND/value0..1 configures raw protocol;
 * its provider does not interpret Camera/UI capabilities or actions. */
/* INPUT_STATE is a lease-free EVENT from the Input domain. The envelope
 * generation is its lifecycle; source_epoch/report_id describe reports, not
 * a Camera safety generation. UI validates the complete value before updates.
 * Battery retains protocol levels 0..10/255. A SIM controller can be connected
 * without an online physical ATOM; a mismatched source cannot be connected. */
typedef struct {
    unsigned fps_tenths, battery, focus, selected, info_level;
    int32_t ev;
    bool settings, failed, default_password, extra_menu, wifi_menu;
} app_ui_state_t;

typedef enum { APP_UI_MENU_HANDLED, APP_UI_MENU_CAMERA, APP_UI_MENU_WIFI } app_ui_menu_route_t;
typedef struct {
    app_ui_menu_route_t route;
    app_camera_property_t property;
    app_ui_state_t state;
} app_ui_menu_result_t;
/* UI_MENU_ACTION carries a pad action. UI owns navigation and resolves a STEP
 * to a semantic property in the reply; Input owns the subsequent Camera
 * command and its safety generation. The Wi-Fi row is information-only;
 * it does not enqueue network changes or persist configuration.
 * RELEASE_ALL may be sent without REQUEST/deadline to cancel the page without
 * blocking Camera's safety release; lifecycle generations still apply.
 * No backend property IDs cross this API. */
typedef enum { APP_UI_PREF_GET, APP_UI_PREF_INFO_SET, APP_UI_PREF_INFO_NEXT,
    APP_UI_PREF_PAD_SET, APP_UI_PREF_RESET, APP_UI_PREF_COUNT } app_ui_preference_op_t;
enum { APP_UI_BENCH_START,APP_UI_BENCH_STATUS };
typedef enum { APP_SYSTEM_MODE_STARTUP,APP_SYSTEM_MODE_NORMAL,APP_SYSTEM_MODE_MAINT,APP_SYSTEM_MODE_ACTIVATING,APP_SYSTEM_MODE_RESTART } app_system_mode_t;
/* DISPLAY_BENCH UART REQUEST uses command.index=START/STATUS and its own
 * nonzero token. START duration_ms=30000 admits a private UI worker; completion
 * EVENT/STATUS payload.benchmark carries the actual result. STATUS resolves
 * an ambiguous admission timeout without restarting a benchmark. Debug only. */
/* UI_PREFERENCES accepts GET only, with no lease and command.flag=false.
 * The current boot snapshot is returned as value=info/direction=pad. A live
 * deadline and nonzero source generation are required. Retired write/reset
 * enum values remain for ABI numbering and return NOT_SUPPORTED; persistence
 * is maintenance Web only. There is no normal preference writer/completion. */

/* WIFI_NETWORK_CHANGED is a lease-free control EVENT from Wi-Fi. Both envelope
 * generation and payload.network.generation contain the new network generation
 * (not a channel token or endpoint epoch). Camera invalidates its active PTP
 * instance; UI resets RSSI. Failed publication retries the same generation.
 * WIFI_CONFIG_GET remains a normal read; PREPARE/COMMIT/CANCEL/RESULT request
 * IDs are retired and return NOT_SUPPORTED. Maintenance uses app_wifi directly. */

typedef union {
    esp_err_t result;
    pad_action_t action;
    gamepad_caps_t capabilities;
    app_camera_status_t camera;
    app_camera_property_state_t property;
    app_input_state_t input;
    i2c_monitor_stats_t i2c_stats;
    struct { i2c_frame_record_t frame; uint32_t dropped; bool available; } i2c;
    app_ui_state_t ui;
    app_ui_menu_result_t menu;
    app_network_config_t config;
    app_network_state_t network;
    struct { app_network_client_t clients[4]; size_t count; } discovery;
    struct { uint8_t mac[6]; int32_t rssi; bool selected; } peer;
    /* Channel tokens belong to source + message generation. Network generation
     * is separate. SEND/RECEIVE require BULK and a lease; RECEIVE requires write
     * permission. RECEIVE poll uses length0/no lease and reports readiness
     * without consuming bytes. Replies transfer the lease back and report partial length and
     * semantic status. A timeout is not buffer ownership return: wait for the
     * lease callback before reusing/freeing the producer buffer. CLOSE cancels
     * active I/O and returns queued leases before its acknowledgement. Wi-Fi
     * retains eight closed ownership records for idempotent close retries. */
    /* CLOSE may use token0 + opening_correlation to cancel an OPEN whose reply
     * has not yet supplied its token. Ownership checks remain identical. */
    struct { uint32_t token, generation, opening_correlation; size_t length; char address[16]; uint16_t port;
        app_network_io_status_t status; bool poll, readable; } channel;
    struct { uint32_t token, value, duration_ms; int32_t direction; unsigned index; bool flag; } command;
    /* CAMERA_FRAME uses command.token (unique in domain generation) and
     * duration_ms (read latency), plus readonly JPEG lease. UI_FRAME_RESULT
     * returns the token/generation/result to Camera; it never owns JPEG bytes.
     * NOT_FINISHED denotes a dropped frame after successful display recovery. */
    struct { unsigned frames, bytes; int64_t elapsed_us; uint32_t token; esp_err_t result; } benchmark;
    struct { unsigned mode; uint32_t uptime_s; size_t free_internal, free_psram; bool restart_pending; } system;
} app_message_payload_t;

typedef struct app_message_lease app_message_lease_t;
typedef struct {
    app_message_type_t type;
    app_endpoint_t source, target;
    uint32_t flags, correlation_id, generation;
    int64_t deadline_us;
    app_message_payload_t payload;
    app_message_lease_t *lease;
    uint32_t endpoint_epoch; /* Router-owned delivery stamp; callers leave zero. */
    esp_err_t result;
} app_message_t;

typedef void (*app_message_lease_return_fn)(void *context);
/* Task-safe, bounded pool; buffer/context remain owned by the producer until
 * the last reference returns. Destructor runs once, outside router locks, on
 * the last releasing task. A failed create leaves ownership with the caller. */
esp_err_t app_message_lease_create(void *buffer, size_t length, bool writable,
    app_message_lease_return_fn returned, void *context, app_message_lease_t **lease);
const void *app_message_lease_data(const app_message_lease_t *lease, size_t *length);
void *app_message_lease_write(app_message_lease_t *lease, size_t *length);
/* Releases only this message's reference and clears its lease. Never manually
 * copy an owned message; router fan-out retains references explicitly. */
void app_message_release(app_message_t *message);

enum { APP_NORMAL_LIVE,APP_NORMAL_SETTINGS,APP_NORMAL_DISPLAY_TEST };
