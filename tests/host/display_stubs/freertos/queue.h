#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
BaseType_t xQueueReceive(QueueHandle_t queue, void *item, TickType_t timeout);
BaseType_t xQueueSend(QueueHandle_t queue, const void *item, TickType_t timeout);
