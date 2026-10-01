#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
// Socket owner only. A rejected value sets accepted=false without failing I/O.
bool sony_set_exposure_mode(int fd, uint32_t transaction, uint32_t value, bool *accepted);
// +1 near / -1 far, confirmed by the user for this camera.
bool sony_manual_focus_step(int fd, uint32_t transaction, int direction, bool *accepted);
