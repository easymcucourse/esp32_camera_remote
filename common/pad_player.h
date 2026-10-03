#pragma once
#include "pad_cmd.h"

typedef struct {
    bool connected;
    uint32_t buttons;
    int8_t lx, ly, rx, ry;
    uint8_t l2, r2, battery;
} pad_state_t;
typedef struct { pad_sequence_t sequence; uint32_t token; } pad_job_t;
typedef struct {
    pad_state_t state;
    pad_job_t jobs[4];
    unsigned head, count, action;
    uint32_t started, due;
    bool running;
} pad_player_t;
typedef struct {
    void (*apply)(void *context, const pad_state_t *state);
    void (*done)(void *context, uint32_t token, uint32_t elapsed, bool cancelled);
    void *context;
} pad_player_ops_t;
bool pad_player_enqueue(pad_player_t *p, const pad_sequence_t *s, uint32_t token);
void pad_player_tick(pad_player_t *p, uint32_t now, const pad_player_ops_t *ops);
/* Release and cancel all work before changing input source. */
void pad_player_cancel(pad_player_t *p, uint32_t now, const pad_player_ops_t *ops);
