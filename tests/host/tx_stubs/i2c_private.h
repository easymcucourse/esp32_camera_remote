#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define SOC_I2C_FIFO_LEN 32
#define pdTRUE 1
#define pdMS_TO_TICKS(value) (value)
typedef struct { uint8_t bytes[32]; size_t count; bool enabled; } fake_fifo_t;
typedef struct { uint8_t bytes[128]; size_t count; bool fail_send; } fake_ring_t;
typedef struct { int spinlock; struct { fake_fifo_t *dev; } hal; } fake_bus_t;
struct fake_slave { fake_bus_t *base; bool *operation_mux; fake_ring_t *tx_ring_buf; };
int xSemaphoreTake(bool *mutex, unsigned timeout);
int xSemaphoreGive(bool *mutex);
void fake_enter(int *lock);
void fake_exit(int *lock);
#define portENTER_CRITICAL(lock) fake_enter(lock)
#define portEXIT_CRITICAL(lock) fake_exit(lock)
