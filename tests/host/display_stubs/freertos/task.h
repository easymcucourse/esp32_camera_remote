#pragma once
#include "FreeRTOS.h"
int xPortGetCoreID(void);
unsigned uxTaskGetStackHighWaterMark(void *task);
