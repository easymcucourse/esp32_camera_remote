#pragma once
#include <stdint.h>
typedef struct { uint8_t before[12];uint16_t chip_id;uint8_t after[10]; } esp_image_header_t;
#define ESP_CHIP_ID_ESP32S3 9
