#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {
    uint8_t guid[16], peer[22]; /* peer = camera MAC[6] + InitCommandAck GUID[16] */
    bool paired;
} camera_identity_t;
bool camera_identity_load(camera_identity_t *identity);
/* Call only after OpenSession + full Sony initialization have succeeded. */
bool camera_identity_confirm(camera_identity_t *identity, const uint8_t mac[6], const uint8_t camera_guid[16]);
/* Caller must exclude concurrent camera tasks. Only sony_remote is erased. */
bool camera_identity_forget(void);
