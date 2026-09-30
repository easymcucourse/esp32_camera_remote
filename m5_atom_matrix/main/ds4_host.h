#pragma once
#include "esp_err.h"
#include "ds4_report.h"
#include "ds4_events.h"

esp_err_t ds4_host_init(void);
void ds4_host_get_state(ds4_state_t *state);
bool ds4_host_read_event(uint32_t ack_id, ds4_event_t *event, uint32_t *dropped);
