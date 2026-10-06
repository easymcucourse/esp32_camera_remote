#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;
enum { I2C_NUM_0, I2C_ADDR_BIT_LEN_7 };
typedef struct { unsigned dev_addr_length,device_address,scl_speed_hz; } i2c_device_config_t;
esp_err_t i2c_master_get_bus_handle(unsigned port,i2c_master_bus_handle_t *bus);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t *config,i2c_master_dev_handle_t *device);
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t device);
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus,unsigned address,unsigned timeout);
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device,const uint8_t *bytes,size_t length,unsigned timeout);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t device,uint8_t *bytes,size_t length,unsigned timeout);
