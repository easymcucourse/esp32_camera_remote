#pragma once
#include "esp_err.h"
#include <stddef.h>
typedef struct { size_t content_len,offset; const unsigned char *body; const char *mime,*image_size; void *user_ctx; char status[40],response[1024]; } httpd_req_t;
#define HTTPD_RESP_USE_STRLEN -1
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
esp_err_t httpd_resp_send_err(httpd_req_t*,int,const char*);
esp_err_t httpd_resp_set_type(httpd_req_t*,const char*);
esp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*);
esp_err_t httpd_resp_set_status(httpd_req_t*,const char*);
esp_err_t httpd_resp_send(httpd_req_t*,const char*,int);
int httpd_req_recv(httpd_req_t*,char*,size_t);
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t*,const char*,char*,size_t);
