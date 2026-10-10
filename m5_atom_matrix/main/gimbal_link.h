#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
esp_err_t gimbal_link_init(void);
uint8_t gimbal_link_state(void);
bool gimbal_link_fault(void);
bool gimbal_link_command(int argc, char **argv);
