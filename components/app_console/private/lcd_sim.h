#pragma once
#include <stdbool.h>
#include "app_message.h"
/* Transitional UART encoders only. Provider state is queried via Console. */
bool lcd_sim_command(int argc,char **argv);
void lcd_sim_event(const app_message_t *message);
bool lcd_sim_enabled(void);
