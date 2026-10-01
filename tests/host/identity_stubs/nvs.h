#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_INVALID_SIZE 2
#define ESP_ERR_NVS_INVALID_LENGTH 3
#define NVS_READWRITE 1
int nvs_open(const char *name,int mode,nvs_handle_t *out);
int nvs_get_blob(nvs_handle_t handle,const char *key,void *data,size_t *size);
int nvs_set_blob(nvs_handle_t handle,const char *key,const void *data,size_t size);
int nvs_commit(nvs_handle_t handle);
int nvs_erase_all(nvs_handle_t handle);
void nvs_close(nvs_handle_t handle);
