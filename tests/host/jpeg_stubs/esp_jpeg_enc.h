#pragma once
#include "esp_jpeg_dec.h"
typedef void *jpeg_enc_handle_t;
#define JPEG_SUBSAMPLE_422 1
typedef struct { unsigned width,height; int src_type,subsampling,quality; } jpeg_enc_config_t;
#define DEFAULT_JPEG_ENC_CONFIG() {0}
jpeg_error_t jpeg_enc_open(jpeg_enc_config_t *,jpeg_enc_handle_t *);
jpeg_error_t jpeg_enc_process(const jpeg_enc_handle_t,const uint8_t *,int,uint8_t *,int,int *);
jpeg_error_t jpeg_enc_close(jpeg_enc_handle_t);
