#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { GIMBAL_NONE, GIMBAL_STOP, GIMBAL_MOVE, GIMBAL_CENTER } gimbal_action_t;
typedef struct { bool connected, l3; int8_t x, y; uint32_t report_ms, epoch; } gimbal_input_t;
typedef struct {
    int8_t offset_x, offset_y;
    bool invert_y;
    uint16_t span, tilt_span; /* tilt_span=0 preserves legacy shared span. */
} gimbal_tuning_t;
typedef struct { gimbal_action_t action; int16_t pan, tilt; } gimbal_output_t;
typedef struct {
    bool linked, armed, l3, moving, centering, sent;
    uint32_t epoch, sent_ms, center_ms;
} gimbal_control_t;
/* Call again after a rejected send: motion/stop state must never be committed on failure. */
gimbal_output_t gimbal_control_step(gimbal_control_t *s, const gimbal_tuning_t *cfg,
                                   const gimbal_input_t *in, bool ready, uint32_t now);
