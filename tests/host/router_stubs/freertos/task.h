#pragma once
#include "FreeRTOS.h"
typedef void (*TaskFunction_t)(void *context);
BaseType_t xTaskCreate(TaskFunction_t function, const char *name, unsigned stack,
    void *context, unsigned priority, void *handle);
void vTaskDelay(TickType_t delay);
void vTaskDelete(void *task);

TickType_t xTaskGetTickCount(void);
void vTaskDelayUntil(TickType_t *previous, TickType_t increment);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait);
