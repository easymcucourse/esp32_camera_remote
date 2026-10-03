#pragma once
#include "atom_client.h"
#include "gamepad_input.h"
typedef struct { atom_client_t client; gamepad_snapshot_t pad; bool sim; } atom_link_status_t;
void atom_link_get_status(atom_link_status_t *out);

void atom_link_start(void);
void atom_link_wake(void);
