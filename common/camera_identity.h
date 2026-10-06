#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t guid[16], peer[22]; /* peer = camera MAC[6] + InitCommandAck GUID[16] */
    bool paired;
} camera_identity_t;
/* Persistent record primitives; no Camera runtime or backend dependencies.
 * The existing sony_remote guid[16]/peer[22] format is unchanged. Core must
 * exclude normal Camera writers before maintenance erases this namespace.
 * Flash operations must run with internal RAM stack/input (normal Camera
 * uses its one-shot identity worker; maintenance uses the HTTP task). */
bool camera_identity_load(camera_identity_t *identity);
/* Call only after OpenSession + full Sony initialization have succeeded. */
bool camera_identity_confirm(camera_identity_t *identity, const uint8_t mac[6], const uint8_t camera_guid[16]);
/* Caller must exclude concurrent storage writers. Only sony_remote is erased. */
bool camera_identity_forget(void);
