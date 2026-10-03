#pragma once
#include "esp_err.h"
#include "ds4_report.h"
#include "ds4_events.h"

esp_err_t ds4_host_init(void);
void ds4_host_get_state(ds4_state_t *state);
bool ds4_host_read_event(uint32_t ack_id, ds4_event_t *event, uint32_t *dropped);
bool ds4_host_poll(uint32_t ack_id, ds4_state_t *snapshot, uint8_t *link_state,
                   ds4_event_t *event, uint8_t *remaining, uint32_t *dropped, uint8_t *source_tag);
void ds4_host_status(uint8_t *link_state, uint32_t *dropped);
void ds4_host_debug_status(ds4_state_t *snapshot, uint8_t *link, unsigned *queued, uint32_t *dropped);
bool ds4_host_sim_active(void);
void ds4_host_set_sim(bool enabled);
void ds4_host_apply_sim(const ds4_state_t *next);
void ds4_host_sim_overflow(void);
