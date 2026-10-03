#pragma once
#include "driver/i2c_slave.h"
#include <stddef.h>
/* Task context, same CPU as slave interrupt. NULL/0 clears all pending TX. */
esp_err_t atom_slave_replace_reply(i2c_slave_dev_handle_t slave, const uint8_t *bytes, size_t size);
