#pragma once
#include "atom_protocol.h"
#include "pad_player.h"
#include "../m5_atom_matrix/main/ds4_events.h"
typedef struct {
    pad_state_t pad;
    ds4_events_t events;
    uint32_t boot_id, fail, crc, timeout;
    uint8_t version, gimbal, generation;
    bool online;
} atom_sim_t;
typedef enum { ATOM_SIM_OK, ATOM_SIM_IO, ATOM_SIM_TIMEOUT } atom_sim_result_t;
void atom_sim_init(atom_sim_t *s);
void atom_sim_pad(atom_sim_t *s, const pad_state_t *pad);
void atom_sim_reboot(atom_sim_t *s);
void atom_sim_gap(atom_sim_t *s, bool overflow);
/* Produces the actual v2 bytes consumed by atom_client; no UI/input bypass. */
atom_sim_result_t atom_sim_transact(atom_sim_t *s, const uint8_t request[ATOM_REQUEST_SIZE],
                                  uint8_t reply[ATOM_RESPONSE_MAX], size_t *size);
