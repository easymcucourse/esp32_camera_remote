#pragma once
#include "../maintenance_stubs/esp_http_server.h"
#include <stdbool.h>
#define HTTPD_408_REQ_TIMEOUT 408
typedef void *httpd_handle_t;
typedef int httpd_err_code_t;
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_405_METHOD_NOT_ALLOWED 405
typedef esp_err_t (*httpd_err_handler_func_t)(httpd_req_t*,httpd_err_code_t);
esp_err_t httpd_register_err_handler(httpd_handle_t,httpd_err_code_t,httpd_err_handler_func_t);
enum { HTTP_GET,HTTP_POST };
typedef struct { const char *uri;int method;esp_err_t (*handler)(httpd_req_t*);void *user_ctx; } httpd_uri_t;
typedef struct { int task_priority,stack_size,max_open_sockets,recv_wait_timeout,send_wait_timeout,max_uri_handlers,server_port;bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
esp_err_t httpd_start(httpd_handle_t*,const httpd_config_t*);
esp_err_t httpd_stop(httpd_handle_t);
esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*);
esp_err_t httpd_get_client_list(httpd_handle_t,size_t*,int*);
esp_err_t httpd_sess_trigger_close(httpd_handle_t,int);
