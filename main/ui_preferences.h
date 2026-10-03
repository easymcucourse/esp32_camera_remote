#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t ui_preferences_start(void);
unsigned ui_preferences_level(void);
/* Ordered asynchronous request; next=true cycles based on the owner's latest value. */
esp_err_t ui_preferences_request(unsigned level,bool next,uint32_t *token);
bool ui_preferences_command(int argc,char **argv);
void ui_preferences_poll(void);
/* Factory reset serializes with persistence and rejects new changes until reboot. */
esp_err_t ui_preferences_reset(void);
