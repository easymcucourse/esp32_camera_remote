#pragma once
#include <stdbool.h>
#include "esp_err.h"
esp_err_t pad_console_start(void);
bool pad_console_command(int argc, char **argv);
void pad_console_poll(void);
