#pragma once
#include "freertos/task.h"
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn,const char *name,unsigned stack,void *context,
    unsigned priority,void *handle,unsigned core,unsigned caps);
void vTaskDeleteWithCaps(void *task);
