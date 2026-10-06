#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define INPUT_PROVIDER_API_VERSION 1
typedef enum { INPUT_SOURCE_ATOM, INPUT_SOURCE_UART_SIM, INPUT_SOURCE_COUNT } input_source_kind_t;
/* Opaque registration identity, never a device/queue/state pointer. */
typedef uint32_t input_provider_handle_t;
typedef enum { INPUT_DISCONNECT_OFFLINE, INPUT_DISCONNECT_RESTART,
    INPUT_DISCONNECT_GAP, INPUT_DISCONNECT_STOP, INPUT_DISCONNECT_OVERFLOW,
    INPUT_DISCONNECT_REASON_COUNT } input_disconnect_reason_t;
typedef struct {
    bool connected, atom_online, mismatch, sim;
    uint32_t buttons, source_epoch, report_id;
    int16_t rx, ry;
    uint8_t lt, rt, battery, kind, gimbal;
    bool gap, event_valid;
    uint32_t event_buttons;
} input_report_t;

/* Task-safe nonblocking copied reports. Battery retains the existing protocol
 * level 0..10 or 255; stick values retain signed int8 range. Epoch/ID are
 * nonzero, monotonic within a registration, and may not wrap. Providers must
 * bump epoch before resetting ID; cached edge validity is separate from the
 * periodic snapshot ID. Registration is one per source kind.
 * Overflow schedules a safety disconnect and invalidates queued reports from
 * that source. A provider restart uses unregister/register, not an old handle.
 * Registration requires the Core-owned input lifecycle to be initialized. */
esp_err_t input_provider_register(input_source_kind_t kind, input_provider_handle_t *handle);
esp_err_t input_provider_publish(input_provider_handle_t handle, const input_report_t *report);
esp_err_t input_provider_disconnect(input_provider_handle_t handle, input_disconnect_reason_t reason);
esp_err_t input_provider_unregister(input_provider_handle_t handle);
