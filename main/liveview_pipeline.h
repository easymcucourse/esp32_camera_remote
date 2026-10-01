#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

// Producer owns the context and buffers until the worker signals done.
typedef struct {
    int slot; // -1 terminates the worker after all submitted frames.
    size_t size;
    int64_t read_ms;
} jpeg_job_t;

typedef struct {
    uint8_t *data[2];
    QueueHandle_t free_slots;
    QueueHandle_t ready;
    SemaphoreHandle_t done;
    atomic_bool failed;
} jpeg_pipeline_t;

// Worker returns slots after publication; it never touches context after done.
void jpeg_decode_task(void *arg);
