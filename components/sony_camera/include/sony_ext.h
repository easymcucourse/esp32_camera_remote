#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Integer scalar encoding for a validated property descriptor (PTP types 1..6).
 * Uses exactly 1/2/4 bytes. Caller validates writability and enum/range bounds. */
bool sony_set_scalar(int fd, uint32_t transaction, uint16_t property,
                     uint16_t type, uint32_t value, bool *accepted);
// Socket owner only. A rejected value sets accepted=false without failing I/O.
bool sony_set_exposure_mode(int fd, uint32_t transaction, uint32_t value, bool *accepted);
// +1 near / -1 far, confirmed by the user for this camera.
bool sony_manual_focus_step(int fd, uint32_t transaction, int direction, bool *accepted);
/* Explicit button states, u16 2=pressed/started and 1=released/stopped. */
bool sony_shutter_button(int fd, uint32_t transaction, bool full, bool pressed, bool *accepted);
bool sony_movie_record(int fd, uint32_t transaction, bool recording, bool *accepted);
/* Minimal continuous speed: int8 +1=Tele, -1=Wide, 0=stop. */
bool sony_zoom(int fd, uint32_t transaction, int direction, bool *accepted);
/* Shutter/aperture relative adjustment, int8 +/-1 with ControlDeviceB.
 * Caller must validate an enabled, non-sentinel camera descriptor. */
bool sony_setting_step(int fd, uint32_t transaction, uint16_t property, int direction, bool *accepted);
