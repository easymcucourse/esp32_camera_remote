#pragma once
#include "../../router_stubs/freertos/task.h"
typedef void *TaskHandle_t;
void xTaskNotifyGive(TaskHandle_t task);
