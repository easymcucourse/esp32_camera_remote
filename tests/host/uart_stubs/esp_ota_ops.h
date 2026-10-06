#pragma once
#include "esp_err.h"
typedef struct { char label[17]; } esp_partition_t;
typedef enum { ESP_OTA_IMG_PENDING_VERIFY,ESP_OTA_IMG_VALID,ESP_OTA_IMG_ABORTED,ESP_OTA_IMG_INVALID } esp_ota_img_states_t;
const esp_partition_t *esp_ota_get_running_partition(void);
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *partition);
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state);
