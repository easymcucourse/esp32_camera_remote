#pragma once
#include "FreeRTOS.h"
TickType_t xTaskGetTickCount(void);
int xPortGetCoreID(void);
unsigned uxTaskGetStackHighWaterMark(void *task);
