#pragma once
#include "esp_err.h"
#include <stdbool.h>
/* Initialize after Bluedroid; this client owns the BLE GAP/GATTC callbacks.
 * Classic DS4 remains owned by ds4_host. Battery is cleared on disconnect. */
esp_err_t ble_gamepad_init(void);
bool ble_gamepad_command(int argc,char **argv);
