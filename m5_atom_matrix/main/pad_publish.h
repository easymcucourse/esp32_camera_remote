#pragma once
#include "ds4_report.h"
#include "ds4_events.h"
typedef struct {
    ds4_state_t state;
    ds4_events_t events;
    unsigned source;
    uint8_t generation;
} pad_publish_t;
enum { PAD_SOURCE_NONE, PAD_SOURCE_DS4, PAD_SOURCE_BLE, PAD_SOURCE_SIM };
unsigned pad_publish_source(bool classic, bool ble);
void pad_publish_apply(pad_publish_t *p, unsigned source, const ds4_state_t *next);
void pad_publish_reset(pad_publish_t *p);

unsigned pad_publish_mode_source(unsigned mode, bool classic, bool ble);
