#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { PAD_ACT_MAX = 32, PAD_HALF = 153, PAD_FULL = 242 };
typedef enum { PAD_PRESS, PAD_RELEASE, PAD_STICK, PAD_TRIGGER, PAD_WAIT } pad_act_kind_t;
typedef struct {
    pad_act_kind_t kind;
    uint32_t mask;
    int16_t x, y;
    uint16_t value;
    uint8_t side;
} pad_act_t;
typedef struct {
    pad_act_t actions[PAD_ACT_MAX];
    unsigned count;
    uint32_t duration_ms;
    bool camera_warning;
} pad_sequence_t;
/* All-or-nothing parse. Input strings are never modified. */
bool pad_cmd_parse(int argc, char **argv, pad_sequence_t *out, const char **error);
uint32_t pad_cmd_button_mask(const char *name);
