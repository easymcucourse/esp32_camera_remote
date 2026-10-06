#include "app_maintenance_web.h"
#include "app_maintenance_ota.h"
#include "app_maintenance.h"
#include "esp_http_server.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "cJSON.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// Production embeds the actual generated gzip. Host tests only validate the
// byte delivery route; HTML script syntax is checked separately.
#ifdef _WIN32
__asm__(".section .rdata\n");
#else
__asm__(".section .rodata\n");
#endif
__asm__(".global _binary_index_html_gz_start\n_binary_index_html_gz_start:\n.byte 31,139\n.global _binary_index_html_gz_end\n_binary_index_html_gz_end:\n.text\n");
static const char *scenario;
static httpd_uri_t routes[15];static unsigned route_count,starts,stops,touches,prepare_count,cancel_count,commit_count,saves,applies,wifi_commits,wifi_cancels;
static int context,network_context;
static unsigned resets,wifi_notifications;
static uint32_t generation=1;
static httpd_err_handler_func_t not_found,wrong_method;
static unsigned exclusive_count,reboot_count,redirect_count;
static unsigned closes;
static network_config_t active;
static bool is(const char *name) { return !strcmp(scenario,name); }
static bool exclusive(void *arg,uint32_t timeout)
{
    assert(arg==&context && timeout==35000);++exclusive_count;
    app_maintenance_status_t state;app_maintenance_get_status(&state);
    assert(state.initialized && state.phase==APP_MAINTENANCE_ACTIVATING);
    httpd_req_t pending={0};assert(not_found(&pending,HTTPD_404_NOT_FOUND)==ESP_FAIL);
    assert(!strcmp(pending.status,"409 Conflict") && exclusive_count==1);
    if(is("trigger_denied")) { /* Owner has not entered exclusivity. */
        app_maintenance_trigger_close();return false;
    }
    if(is("trigger_missing_activation"))return true;
    if(is("trigger_closed_during_claim")) {
        app_maintenance_trigger_close();assert(app_maintenance_activate()==ESP_ERR_INVALID_STATE);
    } else assert(app_maintenance_activate()==ESP_OK);
    return true;
}
static void reboot(void *arg) { assert(arg==&context);++reboot_count; }
static bool available(void *arg) { assert(arg==&context);return !is("unavailable") && !is("factory_unavailable"); }
static void touch(void *arg) { assert(arg==&context);++touches; }
static bool prepare(void *arg) { assert(arg==&context);++prepare_count;return !is("factory_busy"); }
static void cancel(void *arg) { assert(arg==&context);++cancel_count; }
static bool commit(void *arg,unsigned delay) { assert(arg==&context && delay==1500);++commit_count;return true; }
static esp_err_t read_settings(void *arg,app_maintenance_settings_t *settings) { assert(arg==&context);*settings=(app_maintenance_settings_t){1,1,2};return ESP_OK; }
static esp_err_t write_settings(void *arg,const app_maintenance_settings_t *settings)
{ assert(arg==&context && settings->version==1 && settings->controller==0 && settings->info_level==1);++saves;return is("save_failed")?ESP_FAIL:ESP_OK; }
static esp_err_t reset_factory(void *arg,bool all)
{ assert(arg==&context && all==!is("factory_wifi"));++resets;return is("factory_failed")?ESP_FAIL:is("factory_timeout")?ESP_ERR_TIMEOUT:ESP_OK; }
static void wifi_committed(void *arg,uint32_t token) { assert(arg==&context && token==42);++wifi_notifications; }
unsigned app_wifi_api_version(const app_wifi_t *wifi) { assert(wifi==(app_wifi_t *)&network_context);return is("version")?99:APP_WIFI_API_VERSION; }
uint32_t app_wifi_capabilities(const app_wifi_t *wifi) { assert(wifi==(app_wifi_t *)&network_context);return APP_WIFI_CAP_AP|APP_WIFI_CAP_CONFIG_ASYNC; }
app_wifi_result_t app_wifi_get_status(app_wifi_t *wifi,app_wifi_status_t *state) { assert(wifi==(app_wifi_t *)&network_context);*state=(app_wifi_status_t){.generation=generation,.max_channel=11};return APP_WIFI_OK; }
app_wifi_result_t app_wifi_get_clients(app_wifi_t *wifi,app_wifi_client_t *clients,size_t cap,size_t *count)
{ assert(wifi==(app_wifi_t *)&network_context && clients && cap==4);*count=0;return APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_get(app_wifi_t *wifi,network_config_t *config) { assert(wifi==(app_wifi_t *)&network_context);*config=active;return APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_result(app_wifi_t *wifi,uint32_t token,app_wifi_result_t *result)
{ assert(wifi==(app_wifi_t *)&network_context && token==42);*result=APP_WIFI_OK;return APP_WIFI_PENDING; }
app_wifi_result_t app_wifi_config_apply(app_wifi_t *wifi,const network_config_t *config,bool staged,uint32_t *token)
{ assert(wifi==(app_wifi_t *)&network_context && staged && config->channel==11 && !config->show_password);++applies;*token=42;return APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_commit(app_wifi_t *wifi,uint32_t token,unsigned delay)
{ assert(wifi==(app_wifi_t *)&network_context && token==42 && delay==1500);++wifi_commits;return is("wifi_failed_commit")?APP_WIFI_IO:APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_cancel(app_wifi_t *wifi,uint32_t token)
{ assert(wifi==(app_wifi_t *)&network_context && token==42);++wifi_cancels;return APP_WIFI_OK; }
const esp_app_desc_t *esp_app_get_description(void) { static esp_app_desc_t desc={.version="v1"};return &desc; }
const esp_partition_t *esp_ota_get_running_partition(void) { static esp_partition_t partition={"ota_0",6*1024*1024};return &partition; }
const char *app_maintenance_ota_boot_status(void) { return "valid"; }
esp_err_t app_maintenance_ota_check(httpd_req_t *req) { (void)req;return ESP_OK; }
esp_err_t app_maintenance_ota_upload(httpd_req_t *req) { (void)req;return ESP_OK; }
esp_err_t app_maintenance_ota_status(httpd_req_t *req) { (void)req;return ESP_OK; }
size_t heap_caps_get_free_size(unsigned caps) { (void)caps;return 100000; }
int64_t esp_timer_get_time(void) { return 1000000; }
void esp_fill_random(void *data,size_t bytes) { memset(data,1,bytes); }
esp_err_t httpd_sess_trigger_close(httpd_handle_t server,int session)
{
    assert(server==&context && session==41+(int)(closes%3));++closes;
    if(route_count) {
        httpd_req_t incoming={.user_ctx=routes[0].user_ctx};unsigned old=touches;
        assert(routes[0].handler(&incoming)==ESP_FAIL);
        assert(!strcmp(incoming.status,"409 Conflict") && touches==old);
    }
    return is("close_failed")?ESP_FAIL:ESP_OK;
}
esp_err_t httpd_start(httpd_handle_t *server,const httpd_config_t *config)
{ assert(config->server_port==80 && config->stack_size==6144 && config->max_open_sockets==3 && config->task_priority==3);++starts;if(is("start_failed") || is("trigger_start_failed"))return ESP_FAIL;*server=&context;route_count=0;return ESP_OK; }
esp_err_t httpd_stop(httpd_handle_t server) { assert(server==&context);++stops;return is("stop_failed") || is("trigger_stop_failed")?ESP_FAIL:ESP_OK; }
esp_err_t httpd_register_uri_handler(httpd_handle_t server,const httpd_uri_t *route)
{ assert(server==&context && route_count<15);if(is("register_failed") && route_count==3)return ESP_FAIL;routes[route_count++]=*route;return ESP_OK; }
esp_err_t httpd_register_err_handler(httpd_handle_t server,httpd_err_code_t code,httpd_err_handler_func_t handler)
{
    assert(server==&context && handler);
    if(is("trigger_error_register_failed"))return ESP_FAIL;
    if(code==HTTPD_404_NOT_FOUND)not_found=handler;
    else { assert(code==HTTPD_405_METHOD_NOT_ALLOWED);wrong_method=handler; }
    return ESP_OK;
}
esp_err_t httpd_get_client_list(httpd_handle_t server,size_t *count,int *sockets)
{ assert(server==&context && *count==3 && sockets);if(is("client_list_failed"))return ESP_FAIL;*count=3;for(unsigned i=0;i<3;++i)sockets[i]=41+i;return ESP_OK; }
int httpd_req_recv(httpd_req_t *req,char *data,size_t bytes)
{ if(bytes>13)bytes=13;assert(req->offset+bytes<=req->content_len);memcpy(data,req->body+req->offset,bytes);req->offset+=bytes;
  /* Deterministic external-owner stop interleaving; fake SDK join does not
   * model a real HTTP task or prove its timing. */
  if((is("receive_stop") || (is("receive_stop_last") && req->offset==req->content_len)) && !stops)assert(app_maintenance_web_stop()==ESP_OK);
  return (int)bytes; }
esp_err_t httpd_resp_send_err(httpd_req_t *req,int status,const char *text) { snprintf(req->status,sizeof(req->status),"%d",status);snprintf(req->response,sizeof(req->response),"%s",text);return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *type) { (void)req;assert(type);return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *value) { (void)req;assert(strcmp(key,"Access-Control-Allow-Origin") && key && value);if(!strcmp(key,"Location")) { assert(!strcmp(value,"/"));++redirect_count; }return ESP_OK; }
esp_err_t httpd_resp_set_status(httpd_req_t *req,const char *status) { snprintf(req->status,sizeof(req->status),"%s",status);return ESP_OK; }
esp_err_t httpd_resp_send(httpd_req_t *req,const char *text,int bytes)
{ if(bytes==0) { assert(text && !*text && !strcmp(req->status,"302 Found"));return ESP_OK; }if(bytes!=HTTPD_RESP_USE_STRLEN) { assert(bytes==2 && (unsigned char)text[0]==31);return ESP_OK; }snprintf(req->response,sizeof(req->response),"%s",text);return ((is("lost_reply") || is("factory_lost_reply")) && strstr(text,"reboot_in_ms")) || (is("wifi_failed_ack") && strstr(text,"restart_in_ms"))?ESP_FAIL:ESP_OK; }
static httpd_req_t call(const char *path,const char *body)
{
    httpd_req_t req={.content_len=body?strlen(body):0,.body=(const unsigned char *)body};
    for(unsigned i=0;i<route_count;++i)if(!strcmp(routes[i].uri,path) && routes[i].method==(body?HTTP_POST:HTTP_GET)) { req.user_ctx=routes[i].user_ctx;esp_err_t result=routes[i].handler(&req);if(!strncmp(scenario,"trigger_",8) && !strcmp(req.status,"302 Found"))assert(result==ESP_FAIL && req.offset==0);return req; }
    assert(!"missing route");return req;
}
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];network_config_make_default(&active);
    app_maintenance_web_ops_t ops={available,touch,prepare,cancel,commit,read_settings,write_settings,reset_factory,wifi_committed,&context};
    assert(app_maintenance_web_start()==ESP_ERR_INVALID_STATE);
    assert(app_maintenance_web_init(NULL,&ops)==ESP_ERR_INVALID_ARG);
    app_maintenance_web_ops_t missing=ops;missing.factory_reset=NULL;
    assert(app_maintenance_web_init((app_wifi_t *)&network_context,&missing)==ESP_ERR_INVALID_ARG);
    if(is("version")) { assert(app_maintenance_web_init((app_wifi_t *)&network_context,&ops)==ESP_ERR_NOT_SUPPORTED);return 0; }
    assert(app_maintenance_web_init((app_wifi_t *)&network_context,&ops)==ESP_OK);
    assert(app_maintenance_web_init((app_wifi_t *)&network_context,&ops)==ESP_ERR_INVALID_STATE);
    if(!strncmp(scenario,"trigger_",8)) {
        app_maintenance_system_ops_t system={exclusive,reboot,&context};
        assert(app_maintenance_trigger_open()==ESP_ERR_INVALID_STATE);
        assert(app_maintenance_activate()==ESP_ERR_INVALID_STATE);
        assert(app_maintenance_init(NULL,&system)==ESP_ERR_INVALID_ARG);
        assert(app_maintenance_init((app_wifi_t *)&network_context,&system)==ESP_OK);
        assert(app_maintenance_init((app_wifi_t *)&network_context,&system)==ESP_ERR_INVALID_STATE);
        assert(app_maintenance_activate()==ESP_ERR_INVALID_STATE);
        esp_err_t error=app_maintenance_trigger_open();
        app_maintenance_status_t state;
        if(is("trigger_error_register_failed") || is("trigger_start_failed")) {
            assert(error==ESP_FAIL && stops==(is("trigger_start_failed")?0:1));
            app_maintenance_get_status(&state);assert(state.phase==APP_MAINTENANCE_CLOSED);
            assert(app_maintenance_trigger_open()==ESP_ERR_INVALID_STATE);return 0;
        }
        assert(error==ESP_OK && starts==1 && route_count==15 && not_found && wrong_method);
        assert(app_maintenance_trigger_open()==ESP_ERR_INVALID_STATE);
        httpd_req_t request={0};
        if(is("trigger_closed"))app_maintenance_trigger_close();
        if(is("trigger_unknown"))assert(not_found(&request,HTTPD_404_NOT_FOUND)==ESP_FAIL);
        else if(is("trigger_method"))assert(wrong_method(&request,HTTPD_405_METHOD_NOT_ALLOWED)==ESP_FAIL);
        else if(is("trigger_factory"))request=call("/api/factory","{\"scope\":\"all\",\"confirm\":true}");
        else request=call("/api/settings","{\"type\":\"ds\",\"info_level\":1}");
        assert(!saves && !resets && !prepare_count && !touches); /* First request never executes its route. */
        if(is("trigger_denied") || is("trigger_closed") || is("trigger_closed_during_claim") || is("trigger_missing_activation")) {
            assert(!strcmp(request.status,"409 Conflict") && !redirect_count);
            assert(exclusive_count==(is("trigger_closed")?0:1));
            assert(reboot_count==((is("trigger_closed_during_claim") || is("trigger_missing_activation"))?1:0));
        } else {
            assert(!strcmp(request.status,"302 Found") && redirect_count==1 && exclusive_count==1 && !reboot_count);
            app_maintenance_get_status(&state);assert(state.phase==APP_MAINTENANCE_ACTIVE);
            assert(app_maintenance_activate()==ESP_ERR_INVALID_STATE);
            request=call("/api/info",NULL);assert(touches==1 && strstr(request.response,"authentication"));
            request=(httpd_req_t){0};not_found(&request,HTTPD_404_NOT_FOUND);assert(!strcmp(request.status,"404"));
            request=(httpd_req_t){0};wrong_method(&request,HTTPD_405_METHOD_NOT_ALLOWED);assert(!strcmp(request.status,"405"));
            assert(exclusive_count==1 && starts==1 && !stops);
        }
        assert(app_maintenance_stop()==(is("trigger_stop_failed")?ESP_FAIL:ESP_OK) && stops==1);
        if(is("trigger_stop_failed"))assert(app_maintenance_web_start()==ESP_ERR_INVALID_STATE);
        app_maintenance_get_status(&state);assert(state.phase==APP_MAINTENANCE_CLOSED);
        assert(app_maintenance_activate()==ESP_ERR_INVALID_STATE && app_maintenance_trigger_open()==ESP_ERR_INVALID_STATE);
        request=call("/api/info",NULL);assert(!strcmp(request.status,"409 Conflict"));
        return 0;
    }
    app_maintenance_system_ops_t system={exclusive,reboot,&context};
    assert(app_maintenance_init((app_wifi_t *)&network_context,&system)==ESP_OK);
    esp_err_t started=app_maintenance_trigger_open();
    if(is("start_failed")) { assert(started==ESP_FAIL && starts==1 && !stops);return 0; }
    if(is("register_failed")) { assert(started==ESP_FAIL && stops==1);return 0; }
    assert(started==ESP_OK && route_count==15 && app_maintenance_web_start()==ESP_ERR_INVALID_STATE);
    httpd_req_t trigger=call("/",NULL);assert(!strcmp(trigger.status,"302 Found") && !touches);
    if(is("receive_stop") || is("receive_stop_last")) {
        httpd_req_t req=call("/api/settings","{\"type\":\"ds\",\"info_level\":1}");
        assert(!strcmp(req.status,"409 Conflict") && !saves && !prepare_count && !commit_count);
        assert(stops==1 && closes==3);
        req=call("/api/info",NULL);assert(!strcmp(req.status,"409 Conflict"));return 0;
    }
    if(is("close_failed") || is("client_list_failed")) {
        assert(app_maintenance_web_stop()==ESP_OK && stops==1);
        assert(closes==(is("client_list_failed")?0:3));
        assert(app_maintenance_web_stop()==ESP_OK && stops==1);
        httpd_req_t req=call("/api/info",NULL);assert(!strcmp(req.status,"409 Conflict"));return 0;
    }
    if (!strncmp(scenario,"factory_",8)) {
        const char *body=is("factory_wifi") ? "{\"scope\":\"wifi\",\"confirm\":true}" :
            is("factory_missing") ? "{\"scope\":\"all\"}" :
            is("factory_unconfirmed") ? "{\"scope\":\"all\",\"confirm\":false}" :
            is("factory_duplicate") ? "{\"scope\":\"all\",\"scope\":\"wifi\",\"confirm\":true}" :
            is("factory_unknown") ? "{\"scope\":\"camera\",\"confirm\":true}" :
            is("factory_extra") ? "{\"scope\":\"all\",\"confirm\":true,\"extra\":1}" :
            "{\"scope\":\"all\",\"confirm\":true}";
        httpd_req_t req=call("/api/factory",body);
        if (is("factory_busy") || is("factory_unavailable")) {
            assert(!strcmp(req.status,"409 Conflict") && !resets && !commit_count && !cancel_count);
            assert(prepare_count==is("factory_busy"));
        } else if (is("factory_missing") || is("factory_unconfirmed") || is("factory_duplicate") || is("factory_unknown") || is("factory_extra")) {
            assert(!strcmp(req.status,"400 Bad Request") && !resets && !prepare_count && !commit_count);
        } else if (is("factory_failed") || is("factory_timeout")) {
            assert(!strcmp(req.status,is("factory_timeout")?"504 Gateway Timeout":"500 Internal Server Error"));
            assert(resets==1 && prepare_count==1 && cancel_count==1 && !commit_count);
            assert(strstr(req.response,"partially"));
        } else assert(resets==1 && prepare_count==1 && commit_count==1 && !cancel_count && strstr(req.response,"reboot_in_ms"));
        assert(app_maintenance_web_stop()==ESP_OK);return 0;
    }
    for(unsigned i=0;i<route_count;++i)assert(!strstr(routes[i].uri,"login") && !strstr(routes[i].uri,"logout"));
    httpd_req_t req=call("/api/info",NULL);
    if(is("unavailable")) { assert(!strcmp(req.status,"409 Conflict") && !touches);req=call("/api/settings","{\"type\":\"ds\"}");assert(!saves && !prepare_count); }
    else {
        cJSON *json=cJSON_Parse(req.response);assert(json);assert(!strcmp(cJSON_GetObjectItemCaseSensitive(json,"authentication")->valuestring,"none"));assert(!cJSON_GetObjectItemCaseSensitive(json,"camera_state"));cJSON_Delete(json);
        call("/",NULL);
        req=call("/api/settings",NULL);json=cJSON_Parse(req.response);assert(cJSON_GetObjectItemCaseSensitive(json,"info_level")->valueint==2);cJSON_Delete(json);
        req=call("/api/settings",is("duplicate")?"{\"type\":\"ds\",\"type\":\"xbox\"}":"{\"type\":\"ds\",\"info_level\":1}");
        if(is("duplicate"))assert(!strcmp(req.status,"400 Bad Request") && !saves && !prepare_count);
        else if(is("save_failed"))assert(!strcmp(req.status,"500 Internal Server Error") && saves==1 && cancel_count==1 && !commit_count);
        else assert(saves==1 && commit_count==1 && !cancel_count);
        req=call("/api/wifi","{\"channel\":11,\"show_password\":false}");
        assert(applies==1);
        if(is("wifi_failed_ack"))assert(!wifi_commits && wifi_cancels==1);
        else if(is("wifi_failed_commit"))assert(wifi_commits==1 && wifi_cancels==1);
        else assert(wifi_commits==1 && !wifi_cancels);
        assert(wifi_notifications==(!is("wifi_failed_ack") && !is("wifi_failed_commit")));
        req=call("/api/wifi",NULL);json=cJSON_Parse(req.response);assert(json);cJSON_Delete(json);
        ++generation;req=call("/api/wifi",NULL);json=cJSON_Parse(req.response);
        assert(cJSON_GetObjectItemCaseSensitive(json,"network_generation")->valueint==2);
        assert(cJSON_GetObjectItemCaseSensitive(json,"apply_token")->valueint==((is("wifi_failed_ack")||is("wifi_failed_commit"))?0:42));cJSON_Delete(json);
        req=call("/api/wifi","{\"channel\":1.5}");assert(!strcmp(req.status,"400 Bad Request") && applies==1);
        req=call("/api/wifi","{\"show_password\":0}");assert(!strcmp(req.status,"400 Bad Request") && applies==1);
        req=call("/api/reboot","{\"confirm\":false}");assert(!strcmp(req.status,"400 Bad Request"));
        unsigned old=commit_count;call("/api/maint/exit","{\"confirm\":true}");
        if(!is("lost_reply"))assert(commit_count==old+1);else assert(commit_count==old && cancel_count==1);
    }
    esp_err_t stopped=app_maintenance_web_stop();
    if(is("stop_failed")) {
        assert(stopped==ESP_FAIL && app_maintenance_web_start()==ESP_ERR_INVALID_STATE && closes==3);
        unsigned old_saves=saves;
        req=call("/api/settings","{\"type\":\"ds\"}");assert(!strcmp(req.status,"409 Conflict") && saves==old_saves);
        assert(app_maintenance_web_stop()==ESP_FAIL && stops==2 && closes==6);
    }
    else assert(stopped==ESP_OK && app_maintenance_web_start()==ESP_OK);
    return 0;
}
