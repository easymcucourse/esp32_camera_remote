#pragma once
#include <stdint.h>
typedef struct { uint8_t reserved[16];char version[32],project_name[32],time[16],date[16],idf_ver[32];uint8_t tail[112]; } esp_app_desc_t;
const esp_app_desc_t *esp_app_get_description(void);
