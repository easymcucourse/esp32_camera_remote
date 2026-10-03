#pragma once
#include "matrix_model.h"
typedef struct { bool calibration; uint32_t started_ms; uint8_t forced; } matrix_debug_t;
/* Render an overlay without changing the real model or its fault timers. */
void matrix_debug_frame(const matrix_debug_t *debug, const matrix_model_t *model,
                        uint32_t now, uint8_t colors[25]);
