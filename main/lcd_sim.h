#pragma once
#include "esp_err.h"
#include "atom_protocol.h"
esp_err_t lcd_sim_start(void);
bool lcd_sim_enabled(void);
bool lcd_sim_online(void);
uint32_t lcd_sim_epoch(void);
bool lcd_sim_command(int argc,char **argv);
void lcd_sim_poll(void);
esp_err_t lcd_sim_transact(const uint8_t request[ATOM_REQUEST_SIZE],
                           uint8_t reply[ATOM_RESPONSE_MAX],size_t *size);
