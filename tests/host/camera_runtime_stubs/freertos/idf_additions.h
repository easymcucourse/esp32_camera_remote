#pragma once
#include "freertos/task.h"
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t function,const char *name,unsigned stack,
    void *context,unsigned priority,void *handle,unsigned core,unsigned caps);
int xPortGetCoreID(void);
void vTaskDeleteWithCaps(void *task);
