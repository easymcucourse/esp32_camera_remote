#pragma once
#include "i2c_private.h"
void i2c_ll_slave_disable_tx_it(fake_fifo_t *fifo);
void i2c_ll_slave_enable_tx_it(fake_fifo_t *fifo);
void i2c_ll_slave_clear_stretch(fake_fifo_t *fifo);
void i2c_ll_txfifo_rst(fake_fifo_t *fifo);
void i2c_ll_write_txfifo(fake_fifo_t *fifo, uint8_t *bytes, size_t size);
