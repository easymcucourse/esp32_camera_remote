#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t atom_i2c_start(void);
void atom_i2c_ready(void);
void atom_i2c_button(bool pressed, uint16_t count);
bool atom_i2c_online(void);
uint8_t atom_i2c_faults(void);
void atom_i2c_get_status(uint32_t *age_ms, uint32_t *invalid);
bool atom_i2c_debug_command(int argc,char **argv);
void atom_i2c_debug_poll(void);
