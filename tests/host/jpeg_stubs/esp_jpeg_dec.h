#pragma once
#include <stdint.h>
typedef enum { JPEG_ERR_OK, JPEG_ERR_FAIL, JPEG_ERR_NO_MEM } jpeg_error_t;
typedef void *jpeg_dec_handle_t;
#define JPEG_PIXEL_FORMAT_RGB565_LE 1
typedef struct { int output_type; } jpeg_dec_config_t;
#define DEFAULT_JPEG_DEC_CONFIG() {0}
typedef struct { uint16_t width,height; } jpeg_dec_header_info_t;
typedef struct { uint8_t *inbuf; int inbuf_len; uint8_t *outbuf; } jpeg_dec_io_t;
jpeg_error_t jpeg_dec_open(jpeg_dec_config_t *,jpeg_dec_handle_t *);
jpeg_error_t jpeg_dec_parse_header(jpeg_dec_handle_t,jpeg_dec_io_t *,jpeg_dec_header_info_t *);
jpeg_error_t jpeg_dec_get_outbuf_len(jpeg_dec_handle_t,int *);
jpeg_error_t jpeg_dec_process(jpeg_dec_handle_t,jpeg_dec_io_t *);
jpeg_error_t jpeg_dec_close(jpeg_dec_handle_t);
