#pragma once
#include <stddef.h>
#include <stdint.h>
enum { OTA_PREFIX_SIZE=288 };
typedef struct { char version[32],project[32],time[16],date[16]; } ota_header_info_t;
/* Header inspection is not full-image validation; caller must verify checksum/hash. */
const char *ota_header_check(const uint8_t *data,size_t prefix,size_t total,size_t capacity,
                             uint16_t chip,const char *project,ota_header_info_t *out);
uint64_t ota_header_timestamp(const char *date,const char *time);
