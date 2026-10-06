#pragma once
typedef struct { unsigned cores; } esp_chip_info_t;
void esp_chip_info(esp_chip_info_t *chip);
