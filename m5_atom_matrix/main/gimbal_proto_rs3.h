#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { RS3_FRAME_MAX=256 };
typedef struct { uint8_t bytes[RS3_FRAME_MAX]; size_t used; } rs3_stream_t;
typedef void (*rs3_frame_fn)(void *context, const uint8_t *frame, size_t size);
size_t rs3_frame(uint8_t *out, size_t cap, uint16_t seq, uint8_t receiver,
                 uint8_t flags, uint8_t set, uint8_t id, const uint8_t *payload, size_t size);
size_t rs3_stick(uint8_t *out, size_t cap, uint16_t seq, int16_t pan, int16_t tilt);
size_t rs3_center(uint8_t *out, size_t cap, uint16_t seq);
/* Raw signed TLV values only; units/zero must be verified before motion use. */
typedef struct { int16_t pan, tilt, roll; } rs3_pose_raw_t;
bool rs3_pose_raw(const uint8_t *frame, size_t size, rs3_pose_raw_t *pose);
bool rs3_valid(const uint8_t *frame, size_t size);
/* Bounded reassembly: fragmented/coalesced BLE values, header+frame CRC verification. */
void rs3_stream_feed(rs3_stream_t *s, const uint8_t *bytes, size_t size, rs3_frame_fn fn, void *context);
bool rs3_battery(const uint8_t *frame, size_t size, uint8_t *percent);
bool rs3_name(const char *name);
/* Stored peers may advertise without their name; first pairing requires it. */
bool rs3_candidate(bool saved, const uint8_t target[6], const uint8_t address[6], const char *name);
