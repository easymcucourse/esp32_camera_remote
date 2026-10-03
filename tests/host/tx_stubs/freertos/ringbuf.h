#pragma once
#include "i2c_private.h"
void *xRingbufferReceive(fake_ring_t *ring, size_t *size, unsigned timeout);
void vRingbufferReturnItem(fake_ring_t *ring, void *item);
int xRingbufferSend(fake_ring_t *ring, const void *bytes, size_t size, unsigned timeout);
