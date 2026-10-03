#include "maint_mode.h"
#include "maint_auth.h"
#include "maint_json.h"
#include "maint_wifi.h"
#include "app_restart.h"
#include "maint_ota.h"
#include "camera_pair.h"
#include "atom_link.h"
#include "wifi_ap.h"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static httpd_handle_t server;
static SemaphoreHandle_t auth_mutex;
static maint_auth_t auth;
static uint32_t auth_network_generation,last_wifi_token;
static void sync_network_auth(void)
{
    uint32_t generation=wifi_ap_network_generation();
    if (generation!=auth_network_generation) { maint_auth_logout(&auth);auth_network_generation=generation; }
}
extern const uint8_t html_start[] asm("_binary_index_html_gz_start");
extern const uint8_t html_end[] asm("_binary_index_html_gz_end");
static uint32_t random_number(void *context) { (void)context;return esp_random(); }
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static esp_err_t json_reply(httpd_req_t *request,cJSON *body)
{
    if (!body) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    char *text=cJSON_PrintUnformatted(body);cJSON_Delete(body);
    if (!text) return httpd_resp_send_err(request,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    httpd_resp_set_type(request,"application/json");httpd_resp_set_hdr(request,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(request,text,HTTPD_RESP_USE_STRLEN);free(text);return err;
}
static esp_err_t json_error(httpd_req_t *request,const char *status,const char *error)
{
    httpd_resp_set_status(request,status);cJSON *body=cJSON_CreateObject();
    cJSON_AddStringToObject(body,"error",error);return json_reply(request,body);
}
static bool authenticated(httpd_req_t *request)
{
    char header[40];bool valid=false;
    if (httpd_req_get_hdr_value_len(request,"Authorization")==39 &&
        httpd_req_get_hdr_value_str(request,"Authorization",header,sizeof(header))==ESP_OK &&
        !memcmp(header,"Bearer ",7)) {
        xSemaphoreTake(auth_mutex,portMAX_DELAY);sync_network_auth();valid=maint_auth_check(&auth,header+7);xSemaphoreGive(auth_mutex);
    }
    if (!valid) json_error(request,"401 Unauthorized","unauthorized");
    else maint_mode_touch();
    return valid;
}
static esp_err_t page(httpd_req_t *request)
{
    httpd_resp_set_type(request,"text/html; charset=utf-8");
    httpd_resp_set_hdr(request,"Content-Encoding","gzip");httpd_resp_set_hdr(request,"Cache-Control","no-store");
    return httpd_resp_send(request,(const char*)html_start,html_end-html_start);
}
static cJSON *read_json(httpd_req_t *request)
{
    if (request->content_len>512) { json_error(request,"413 Content Too Large","JSON limit is 512 bytes");return NULL; }
    if (!request->content_len) { json_error(request,"400 Bad Request","missing JSON body");return NULL; }
    char body[513];size_t at=0;
    while (at<request->content_len) {
        int received=httpd_req_recv(request,body+at,request->content_len-at);
        if (received<=0) { httpd_resp_send_err(request,HTTPD_408_REQ_TIMEOUT,"incomplete body");return NULL; }
        at+=received;
    }
    body[at]=0;
    if (!maint_json_flat(body,at)) { json_error(request,"400 Bad Request","expected a flat JSON object without NUL");return NULL; }
    cJSON *json=cJSON_ParseWithLengthOpts(body,at+1,NULL,true);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json);json_error(request,"400 Bad Request","invalid JSON object");return NULL; }
    return json;
}
static esp_err_t login(httpd_req_t *request)
{
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    cJSON *pin=cJSON_GetObjectItemCaseSensitive(json,"pin");
    if (!cJSON_IsObject(json) || !cJSON_IsString(pin)) { cJSON_Delete(json);return json_error(request,"400 Bad Request","invalid JSON PIN"); }
    char token[33];
    xSemaphoreTake(auth_mutex,portMAX_DELAY);
    sync_network_auth();
    maint_auth_result_t result=maint_auth_login(&auth,pin->valuestring,now_ms(),random_number,NULL,token);
    unsigned retry=maint_auth_retry_after(&auth,now_ms());
    xSemaphoreGive(auth_mutex);cJSON_Delete(json);
    cJSON *reply=cJSON_CreateObject();
    if (result==MAINT_AUTH_OK) { cJSON_AddStringToObject(reply,"token",token);maint_mode_touch(); }
    else {
        httpd_resp_set_status(request,result==MAINT_AUTH_LOCKED?"429 Too Many Requests":"401 Unauthorized");
        cJSON_AddStringToObject(reply,"error",result==MAINT_AUTH_LOCKED?"locked":"invalid PIN");
        if (result==MAINT_AUTH_LOCKED) cJSON_AddNumberToObject(reply,"retry_after",retry);
    }
    return json_reply(request,reply);
}
static esp_err_t logout(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    xSemaphoreTake(auth_mutex,portMAX_DELAY);maint_auth_logout(&auth);xSemaphoreGive(auth_mutex);
    return json_reply(request,cJSON_CreateObject());
}
static esp_err_t info(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    const esp_app_desc_t *app=esp_app_get_description();
    const esp_partition_t *partition=esp_ota_get_running_partition();
    camera_debug_status_t camera;camera_debug_get_status(&camera);
    atom_link_status_t atom;atom_link_get_status(&atom);
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"version",app->version);
    char build[40];snprintf(build,sizeof(build),"%s %s",app->date,app->time);
    cJSON_AddStringToObject(reply,"build_time",build);cJSON_AddStringToObject(reply,"idf_version",app->idf_ver);
    cJSON_AddStringToObject(reply,"running_partition",partition?partition->label:"unknown");
    cJSON_AddStringToObject(reply,"last_ota",maint_ota_boot_status());
    cJSON_AddNumberToObject(reply,"uptime_s",esp_timer_get_time()/1000000);
    cJSON_AddNumberToObject(reply,"free_internal",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(reply,"free_psram",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddStringToObject(reply,"camera_state",camera.session?"connected":camera.stopped?"stopped":camera.phase);
    cJSON_AddStringToObject(reply,"atom_state",atom.client.online?"connected":"disconnected");
    cJSON *clients=cJSON_AddArrayToObject(reply,"clients");wifi_ap_client_t list[WIFI_AP_CLIENT_CAPACITY];size_t count=0;
    wifi_ap_get_clients(list,WIFI_AP_CLIENT_CAPACITY,&count);
    for (size_t i=0;i<count;++i) {
        cJSON *client=cJSON_CreateObject();char mac[18];
        snprintf(mac,sizeof(mac),"%02x:%02x:%02x:%02x:%02x:%02x",list[i].mac[0],list[i].mac[1],list[i].mac[2],list[i].mac[3],list[i].mac[4],list[i].mac[5]);
        cJSON_AddStringToObject(client,"mac",mac);cJSON_AddStringToObject(client,"ip",list[i].ip);
        cJSON_AddNumberToObject(client,"rssi",list[i].rssi);cJSON_AddItemToArray(clients,client);
    }
    return json_reply(request,reply);
}
static esp_err_t exit_mode(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    esp_err_t err=json_reply(request,cJSON_CreateObject());
    if (err==ESP_OK) maint_mode_request_off();
    return err;
}
static esp_err_t ota_check(httpd_req_t *req)
{ return authenticated(req)?maint_ota_check(req):ESP_OK; }
static esp_err_t ota_upload(httpd_req_t *req)
{ return authenticated(req)?maint_ota_upload(req):ESP_OK; }
static esp_err_t ota_status(httpd_req_t *req)
{ return authenticated(req)?maint_ota_status(req):ESP_OK; }
static esp_err_t reboot_device(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    bool valid=json->child && !json->child->next && !strcmp(json->child->string,"confirm") && cJSON_IsTrue(json->child);
    cJSON_Delete(json);
    if (!valid) return json_error(request,"400 Bad Request","confirm must be true");
    if (!app_restart_prepare()) return json_error(request,"409 Conflict","restart already pending");
    cJSON *reply=cJSON_CreateObject();cJSON_AddNumberToObject(reply,"reboot_in_ms",1500);
    esp_err_t sent=json_reply(request,reply);
    if (sent!=ESP_OK || !app_restart_commit(1500)) app_restart_cancel();
    return sent;
}
static esp_err_t wifi_get(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    app_wifi_config_t current;wifi_ap_get_config(&current);
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"ssid",current.ssid);cJSON_AddNumberToObject(reply,"channel",current.channel);
    cJSON_AddNumberToObject(reply,"password_len",strlen(current.password));
    cJSON_AddBoolToObject(reply,"show_password",current.show_password);cJSON_AddNumberToObject(reply,"max_channel",wifi_ap_max_channel());
    esp_err_t result=ESP_OK,state=last_wifi_token?wifi_ap_request_result(last_wifi_token,&result):ESP_OK;
    const char *name=!last_wifi_token?"idle":state==ESP_ERR_NOT_FINISHED?"pending":state==ESP_OK&&result==ESP_OK?"done":"failed";
    cJSON_AddStringToObject(reply,"apply_state",name);cJSON_AddNumberToObject(reply,"apply_token",last_wifi_token);
    if (!strcmp(name,"failed")) cJSON_AddStringToObject(reply,"apply_error",esp_err_to_name(state==ESP_OK?result:state));
    return json_reply(request,reply);
}
static esp_err_t wifi_field_error(httpd_req_t *request,const char *field,const char *error)
{
    httpd_resp_set_status(request,"400 Bad Request");cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"field",field);cJSON_AddStringToObject(reply,"error",error);
    return json_reply(request,reply);
}
static esp_err_t wifi_post(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    maint_wifi_patch_t patch={0};unsigned seen=0;const char *field=NULL,*invalid=NULL;
    for (cJSON *item=json->child;item;item=item->next) {
        unsigned bit=0;field=item->string;
        if (!strcmp(field,"ssid")) { bit=1;if (cJSON_IsString(item)) patch.ssid=item->valuestring;else invalid="SSID must be a string"; }
        else if (!strcmp(field,"password")) { bit=2;if (cJSON_IsString(item)) patch.password=item->valuestring;else invalid="password must be a string"; }
        else if (!strcmp(field,"channel")) { bit=4;patch.has_channel=true;if (cJSON_IsNumber(item)) patch.channel=item->valuedouble;else invalid="channel must be an integer"; }
        else invalid="unknown field";
        if (seen&bit) invalid="duplicate field";
        seen|=bit;if (invalid) break;
    }
    if (invalid) { esp_err_t err=wifi_field_error(request,field,invalid);cJSON_Delete(json);return err; }
    app_wifi_config_t current,next;wifi_ap_get_config(&current);
    wifi_cfg_error_t error=maint_wifi_patch(&current,&patch,wifi_ap_max_channel(),&next,&field);
    if (error!=WIFI_CFG_OK) { esp_err_t err=wifi_field_error(request,field,wifi_config_error_text(error));cJSON_Delete(json);return err; }
    cJSON_Delete(json);
    bool change=!wifi_config_equal(&current,&next);bool restart=!wifi_config_network_equal(&current,&next);
    uint32_t token=0;
    if (change && wifi_ap_prepare_apply(&next,&token)!=ESP_OK) return json_error(request,"409 Conflict","Wi-Fi request queue busy");
    cJSON *reply=cJSON_CreateObject();cJSON_AddNumberToObject(reply,"restart_in_ms",restart?1500:0);
    cJSON_AddNumberToObject(reply,"token",token);cJSON_AddStringToObject(reply,"state",change?"pending":"done");
    esp_err_t sent=json_reply(request,reply);
    if (change) {
        if (sent==ESP_OK && wifi_ap_commit_apply(token,1500)==ESP_OK) last_wifi_token=token;
        else wifi_ap_cancel_apply(token);
    }
    memset(next.password,0,sizeof(next.password));return sent;
}
static esp_err_t wifi_random(httpd_req_t *request)
{
    if (!authenticated(request)) return ESP_OK;
    char password[WIFI_PASSWORD_MAX+1];wifi_config_make_password(password,esp_fill_random);
    cJSON *reply=cJSON_CreateObject();cJSON_AddStringToObject(reply,"password",password);
    memset(password,0,sizeof(password));return json_reply(request,reply);
}
void maint_web_pin(char pin[7])
{
    if (!auth_mutex) { pin[0]=0;return; }
    xSemaphoreTake(auth_mutex,portMAX_DELAY);memcpy(pin,auth.pin,7);xSemaphoreGive(auth_mutex);
}
esp_err_t maint_web_start(void)
{
    if (server) return ESP_ERR_INVALID_STATE;
    if (!auth_mutex) auth_mutex=xSemaphoreCreateMutex();
    if (!auth_mutex) return ESP_ERR_NO_MEM;
    last_wifi_token=0;
    xSemaphoreTake(auth_mutex,portMAX_DELAY);maint_auth_reset(&auth,random_number,NULL);
    auth_network_generation=wifi_ap_network_generation();xSemaphoreGive(auth_mutex);
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.task_priority=3;config.stack_size=6144;config.max_open_sockets=3;
    config.lru_purge_enable=true;config.recv_wait_timeout=10;config.send_wait_timeout=10;
    config.max_uri_handlers=12;
    esp_err_t err=httpd_start(&server,&config);
    if (err!=ESP_OK) { server=NULL;return err; }
    const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=page},
        {.uri="/api/login",.method=HTTP_POST,.handler=login},
        {.uri="/api/logout",.method=HTTP_POST,.handler=logout},
        {.uri="/api/info",.method=HTTP_GET,.handler=info},
        {.uri="/api/wifi",.method=HTTP_GET,.handler=wifi_get},
        {.uri="/api/wifi",.method=HTTP_POST,.handler=wifi_post},
        {.uri="/api/wifi/random_password",.method=HTTP_GET,.handler=wifi_random},
        {.uri="/api/maint/exit",.method=HTTP_POST,.handler=exit_mode},
        {.uri="/api/reboot",.method=HTTP_POST,.handler=reboot_device},
        {.uri="/api/ota/check",.method=HTTP_POST,.handler=ota_check},
        {.uri="/api/ota",.method=HTTP_POST,.handler=ota_upload},
        {.uri="/api/ota/status",.method=HTTP_GET,.handler=ota_status},
    };
    for (unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i) {
        err=httpd_register_uri_handler(server,&routes[i]);
        if (err!=ESP_OK) { maint_web_stop();return err; }
    }
    return ESP_OK;
}
esp_err_t maint_web_stop(void)
{
    /* Wake a stalled body receive/send before waiting for the server task. */
    if (server) {
        int sockets[3];size_t count=3;
        if (httpd_get_client_list(server,&count,sockets)==ESP_OK)
            for (size_t i=0;i<count;++i) shutdown(sockets[i],SHUT_RDWR);
    }
    esp_err_t err=server?httpd_stop(server):ESP_OK;
    if (err==ESP_OK) {
        server=NULL;
        if (auth_mutex) { xSemaphoreTake(auth_mutex,portMAX_DELAY);memset(&auth,0,sizeof(auth));xSemaphoreGive(auth_mutex); }
    }
    return err;
}
