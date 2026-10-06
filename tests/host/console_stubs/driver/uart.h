#pragma once
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#define UART_NUM_0 0
esp_err_t uart_driver_install(int,unsigned,unsigned,unsigned,void*,unsigned);
esp_err_t uart_driver_delete(int);
int uart_read_bytes(int,void*,unsigned,TickType_t);
