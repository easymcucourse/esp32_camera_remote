#include "app_maintenance_web.h"
#include "maint_json.h"
#include "maint_wifi.h"
#include "app_maintenance_ota.h"
#include "maintenance_trigger.h"
#include "maintenance_web_internal.h"
#include "esp_http_server.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static httpd_handle_t server;
static atomic_bool stopping;
static app_wifi_t *network;
static app_maintenance_web_ops_t system_ops;
static uint32_t last_wifi_token;
bool maintenance_web_uses_network(const app_wifi_t *wifi)
{ return wifi && network==wifi; }
esp_err_t app_maintenance_web_init(app_wifi_t *wifi,const app_maintenance_web_ops_t *ops)
{
    if (!wifi || !ops || !ops->available || !ops->touch || !ops->restart_prepare ||
        !ops->restart_cancel || !ops->restart_commit || !ops->settings_read || !ops->settings_write || !ops->factory_reset || !ops->wifi_committed)
        return ESP_ERR_INVALID_ARG;
    if (network) return ESP_ERR_INVALID_STATE;
    if (app_wifi_api_version(wifi)!=APP_WIFI_API_VERSION ||
        (app_wifi_capabilities(wifi)&(APP_WIFI_CAP_AP|APP_WIFI_CAP_CONFIG_ASYNC))!=(APP_WIFI_CAP_AP|APP_WIFI_CAP_CONFIG_ASYNC))
        return ESP_ERR_NOT_SUPPORTED;
    network=wifi;system_ops=*ops;return ESP_OK;
}
static unsigned max_channel(void)
{
    app_wifi_status_t state={0};app_wifi_get_status(network,&state);
    return state.max_channel?state.max_channel:13;
}
extern const uint8_t html_start[] asm("_binary_index_html_gz_start");
extern const uint8_t html_end[] asm("_binary_index_html_gz_end");
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
static bool available(httpd_req_t *request)
{
    if (atomic_load(&stopping) || !system_ops.available(system_ops.context)) { json_error(request,"409 Conflict","maintenance unavailable");return false; }
    system_ops.touch(system_ops.context);return true;
}
static esp_err_t missing_route(httpd_req_t *request,httpd_err_code_t error)
{
    if (atomic_load(&stopping)) { json_error(request,"409 Conflict","maintenance stopping");return ESP_FAIL; }
    if (!maintenance_trigger_route(request)) return ESP_FAIL;
    return httpd_resp_send_err(request,error,error==HTTPD_404_NOT_FOUND ? "not found" : "method not allowed");
}
static esp_err_t dispatch(httpd_req_t *request)
{
    if (atomic_load(&stopping)) { json_error(request,"409 Conflict","maintenance stopping");return ESP_FAIL; }
    /* Return failure after the completed trigger response so SDK closes the
     * connection rather than purging an arbitrary POST/OTA body. Merely adding
     * Connection:close does not stop SDK's unread-body purge. */
    if (!maintenance_trigger_route(request)) return ESP_FAIL;
    const httpd_uri_t *route=request->user_ctx;
    return route->handler(request);
}
static esp_err_t page(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
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
        if (atomic_load(&stopping)) { json_error(request,"409 Conflict","maintenance stopping");return NULL; }
        int received=httpd_req_recv(request,body+at,request->content_len-at);
        if (atomic_load(&stopping)) { json_error(request,"409 Conflict","maintenance stopping");return NULL; }
        if (received<=0) { httpd_resp_send_err(request,HTTPD_408_REQ_TIMEOUT,"incomplete body");return NULL; }
        at+=received;
    }
    body[at]=0;
    if (!maint_json_flat(body,at)) { json_error(request,"400 Bad Request","expected a flat JSON object without NUL");return NULL; }
    cJSON *json=cJSON_ParseWithLengthOpts(body,at+1,NULL,true);
    if (!cJSON_IsObject(json)) { cJSON_Delete(json);json_error(request,"400 Bad Request","invalid JSON object");return NULL; }
    return json;
}
static esp_err_t info(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    const esp_app_desc_t *app=esp_app_get_description();
    const esp_partition_t *partition=esp_ota_get_running_partition();
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"version",app->version);
    char build[40];snprintf(build,sizeof(build),"%s %s",app->date,app->time);
    cJSON_AddStringToObject(reply,"build_time",build);cJSON_AddStringToObject(reply,"idf_version",app->idf_ver);
    cJSON_AddStringToObject(reply,"running_partition",partition?partition->label:"unknown");
    cJSON_AddStringToObject(reply,"last_ota",app_maintenance_ota_boot_status());
    cJSON_AddNumberToObject(reply,"uptime_s",esp_timer_get_time()/1000000);
    cJSON_AddNumberToObject(reply,"free_internal",heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(reply,"free_psram",heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_AddStringToObject(reply,"authentication","none");
    cJSON_AddStringToObject(reply,"mode","maintenance");
    cJSON *clients=cJSON_AddArrayToObject(reply,"clients");app_wifi_client_t list[APP_WIFI_CLIENT_CAPACITY];size_t count=0;
    app_wifi_get_clients(network,list,APP_WIFI_CLIENT_CAPACITY,&count);
    for (size_t i=0;i<count;++i) {
        cJSON *client=cJSON_CreateObject();char mac[18];
        snprintf(mac,sizeof(mac),"%02x:%02x:%02x:%02x:%02x:%02x",list[i].mac[0],list[i].mac[1],list[i].mac[2],list[i].mac[3],list[i].mac[4],list[i].mac[5]);
        cJSON_AddStringToObject(client,"mac",mac);cJSON_AddStringToObject(client,"ip",list[i].ip);
        cJSON_AddNumberToObject(client,"rssi",list[i].rssi);cJSON_AddItemToArray(clients,client);
    }
    return json_reply(request,reply);
}
static esp_err_t ota_check(httpd_req_t *req)
{ return available(req)?app_maintenance_ota_check(req):ESP_OK; }
static esp_err_t ota_upload(httpd_req_t *req)
{ return available(req)?app_maintenance_ota_upload(req):ESP_OK; }
static esp_err_t ota_status(httpd_req_t *req)
{ return available(req)?app_maintenance_ota_status(req):ESP_OK; }
static esp_err_t reboot_device(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    bool valid=json->child && !json->child->next && !strcmp(json->child->string,"confirm") && cJSON_IsTrue(json->child);
    cJSON_Delete(json);
    if (!valid) return json_error(request,"400 Bad Request","confirm must be true");
    if (!system_ops.restart_prepare(system_ops.context)) return json_error(request,"409 Conflict","restart already pending");
    cJSON *reply=cJSON_CreateObject();cJSON_AddNumberToObject(reply,"reboot_in_ms",1500);
    esp_err_t sent=json_reply(request,reply);
    if (sent!=ESP_OK || !system_ops.restart_commit(system_ops.context,1500)) system_ops.restart_cancel(system_ops.context);
    return sent;
}
static esp_err_t factory_reset(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    unsigned seen=0;bool all=false,valid=true;
    for (cJSON *item=json->child;item;item=item->next) {
        unsigned bit=0;
        if (!strcmp(item->string,"confirm")) { bit=1;valid=cJSON_IsTrue(item); }
        else if (!strcmp(item->string,"scope")) {
            bit=2;valid=cJSON_IsString(item) &&
                (!strcmp(item->valuestring,"wifi") || !strcmp(item->valuestring,"all"));
            if (valid) all=!strcmp(item->valuestring,"all");
        } else valid=false;
        if (!valid || (seen&bit)) { valid=false;break; }
        seen|=bit;
    }
    cJSON_Delete(json);
    if (!valid || seen!=3) return json_error(request,"400 Bad Request","confirm must be true; scope must be wifi or all");
    if (!system_ops.restart_prepare(system_ops.context)) return json_error(request,"409 Conflict","restart already pending");
    esp_err_t reset=system_ops.factory_reset(system_ops.context,all);
    if (reset!=ESP_OK) {
        system_ops.restart_cancel(system_ops.context);
        return json_error(request,reset==ESP_ERR_TIMEOUT ? "504 Gateway Timeout" : "500 Internal Server Error",
            "factory reset failed; persistent settings may be partially changed");
    }
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"scope",all ? "all" : "wifi");
    cJSON_AddNumberToObject(reply,"reboot_in_ms",1500);
    esp_err_t sent=json_reply(request,reply);
    /* Persistence succeeded. A lost final ACK must not resume writers or
     * suppress reboot into the newly saved defaults. */
    system_ops.restart_commit(system_ops.context,1500);
    return sent;
}
static esp_err_t settings_get(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    app_maintenance_settings_t settings={0};
    if (system_ops.settings_read(system_ops.context,&settings)!=ESP_OK)
        return json_error(request,"500 Internal Server Error","settings unavailable");
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddNumberToObject(reply,"schema_version",settings.version);
    cJSON_AddStringToObject(reply,"type",settings.controller?"xbox":"ds");
    cJSON_AddNumberToObject(reply,"info_level",settings.info_level);
    return json_reply(request,reply);
}
static esp_err_t settings_post(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    app_maintenance_settings_t settings={0};
    if (system_ops.settings_read(system_ops.context,&settings)!=ESP_OK) {
        cJSON_Delete(json);return json_error(request,"500 Internal Server Error","settings unavailable");
    }
    unsigned seen=0;bool valid=json->child!=NULL;
    for(cJSON *item=json->child;valid && item;item=item->next) {
        unsigned bit=0;
        if(!strcmp(item->string,"type")) {
            bit=1;valid=cJSON_IsString(item) && (!strcmp(item->valuestring,"ds") || !strcmp(item->valuestring,"xbox"));
            if(valid)settings.controller=!strcmp(item->valuestring,"xbox");
        } else if(!strcmp(item->string,"info_level")) {
            bit=2;valid=cJSON_IsNumber(item) && item->valuedouble>=0 && item->valuedouble<=2 && item->valuedouble==(unsigned)item->valuedouble;
            if(valid)settings.info_level=(unsigned)item->valuedouble;
        } else valid=false;
        if(seen&bit)valid=false;
        seen|=bit;
    }
    cJSON_Delete(json);
    if(!valid)return json_error(request,"400 Bad Request","invalid or duplicate settings field");
    if(!system_ops.restart_prepare(system_ops.context))return json_error(request,"409 Conflict","restart already pending");
    esp_err_t saved=system_ops.settings_write(system_ops.context,&settings);
    if(saved!=ESP_OK) {
        system_ops.restart_cancel(system_ops.context);return json_error(request,"500 Internal Server Error","failed to save settings");
    }
    cJSON *reply=cJSON_CreateObject();cJSON_AddNumberToObject(reply,"reboot_in_ms",1500);
    esp_err_t sent=json_reply(request,reply);
    /* Persisted settings take effect only after reboot, even when ACK is lost. */
    system_ops.restart_commit(system_ops.context,1500);return sent;
}
static esp_err_t wifi_get(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    network_config_t current;
    if(app_wifi_config_get(network,&current)!=APP_WIFI_OK)return json_error(request,"500 Internal Server Error","network unavailable");
    cJSON *reply=cJSON_CreateObject();
    cJSON_AddStringToObject(reply,"ssid",current.ssid);cJSON_AddNumberToObject(reply,"channel",current.channel);
    cJSON_AddNumberToObject(reply,"password_len",strlen(current.password));
    cJSON_AddBoolToObject(reply,"show_password",current.show_password);cJSON_AddNumberToObject(reply,"max_channel",max_channel());
    app_wifi_status_t net={0};app_wifi_get_status(network,&net);
    /* A committed apply may change generation before the client reconnects.
     * Keep its result token queryable; generation does not erase job history. */
    app_wifi_result_t result=APP_WIFI_OK,state=last_wifi_token?app_wifi_config_result(network,last_wifi_token,&result):APP_WIFI_OK;
    const char *name=!last_wifi_token?"idle":state==APP_WIFI_PENDING?"pending":state==APP_WIFI_OK && result==APP_WIFI_OK?"done":"failed";
    cJSON_AddStringToObject(reply,"apply_state",name);cJSON_AddNumberToObject(reply,"apply_token",last_wifi_token);
    cJSON_AddNumberToObject(reply,"network_generation",net.generation);
    if(!strcmp(name,"failed"))cJSON_AddStringToObject(reply,"apply_error","network apply failed");
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
    if (!available(request)) return ESP_OK;
    cJSON *json=read_json(request);if (!json) return ESP_OK;
    maint_wifi_patch_t patch={0};unsigned seen=0;const char *field=NULL,*invalid=NULL;
    for (cJSON *item=json->child;item;item=item->next) {
        unsigned bit=0;field=item->string;
        if (!strcmp(field,"ssid")) { bit=1;if (cJSON_IsString(item)) patch.ssid=item->valuestring;else invalid="SSID must be a string"; }
        else if (!strcmp(field,"password")) { bit=2;if (cJSON_IsString(item)) patch.password=item->valuestring;else invalid="password must be a string"; }
        else if (!strcmp(field,"channel")) { bit=4;patch.has_channel=true;if (cJSON_IsNumber(item)) patch.channel=item->valuedouble;else invalid="channel must be an integer"; }
        else if (!strcmp(field,"show_password")) { bit=8;patch.has_show_password=true;if(cJSON_IsBool(item))patch.show_password=cJSON_IsTrue(item);else invalid="show_password must be boolean"; }
        else invalid="unknown field";
        if (seen&bit) invalid="duplicate field";
        seen|=bit;if (invalid) break;
    }
    if (invalid) { esp_err_t err=wifi_field_error(request,field,invalid);cJSON_Delete(json);return err; }
    network_config_t current,next;
    if(app_wifi_config_get(network,&current)!=APP_WIFI_OK) { cJSON_Delete(json);return json_error(request,"500 Internal Server Error","network unavailable"); }
    network_cfg_error_t error=maint_wifi_patch(&current,&patch,max_channel(),&next,&field);
    if (error!=NETWORK_CFG_OK) { esp_err_t err=wifi_field_error(request,field,network_config_error_text(error));cJSON_Delete(json);return err; }
    cJSON_Delete(json);
    bool change=!network_config_equal(&current,&next);bool restart=!network_config_network_equal(&current,&next);
    uint32_t token=0;
    if (change && !system_ops.restart_prepare(system_ops.context)) return json_error(request,"409 Conflict","restart already pending");
    if (change && app_wifi_config_apply(network,&next,true,&token)!=APP_WIFI_OK) {
        system_ops.restart_cancel(system_ops.context);return json_error(request,"409 Conflict","Wi-Fi request queue busy");
    }
    cJSON *reply=cJSON_CreateObject();cJSON_AddNumberToObject(reply,"restart_in_ms",restart?1500:0);
    cJSON_AddNumberToObject(reply,"token",token);cJSON_AddStringToObject(reply,"state",change?"pending":"done");
    cJSON_AddBoolToObject(reply,"reboot_after_apply",change);
    esp_err_t sent=json_reply(request,reply);
    if (change) {
        if (sent==ESP_OK && app_wifi_config_commit(network,token,1500)==APP_WIFI_OK) {
            last_wifi_token=token;system_ops.wifi_committed(system_ops.context,token);
        } else { app_wifi_config_cancel(network,token);system_ops.restart_cancel(system_ops.context); }
    }
    memset(next.password,0,sizeof(next.password));memset(current.password,0,sizeof(current.password));return sent;
}
static esp_err_t wifi_random(httpd_req_t *request)
{
    if (!available(request)) return ESP_OK;
    char password[NETWORK_PASSWORD_MAX+1];network_config_make_password(password,esp_fill_random);
    cJSON *reply=cJSON_CreateObject();cJSON_AddStringToObject(reply,"password",password);
    memset(password,0,sizeof(password));return json_reply(request,reply);
}
esp_err_t app_maintenance_web_start(void)
{
    if (!network || server) return ESP_ERR_INVALID_STATE;
    last_wifi_token=0;
    httpd_config_t config=HTTPD_DEFAULT_CONFIG();
    config.task_priority=3;config.stack_size=6144;config.max_open_sockets=3;
    config.lru_purge_enable=true;config.recv_wait_timeout=10;config.send_wait_timeout=10;
    config.max_uri_handlers=15;config.server_port=80;
    esp_err_t err=httpd_start(&server,&config);
    if (err!=ESP_OK) { server=NULL;return err; }
    atomic_store(&stopping,false);
    err=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,missing_route);
    if (err==ESP_OK) err=httpd_register_err_handler(server,HTTPD_405_METHOD_NOT_ALLOWED,missing_route);
    if (err!=ESP_OK) { app_maintenance_web_stop();return err; }
    static const httpd_uri_t routes[]={
        {.uri="/",.method=HTTP_GET,.handler=page},
        {.uri="/api/info",.method=HTTP_GET,.handler=info},
        {.uri="/api/controller",.method=HTTP_GET,.handler=settings_get},
        {.uri="/api/controller",.method=HTTP_POST,.handler=settings_post},
        {.uri="/api/wifi",.method=HTTP_GET,.handler=wifi_get},
        {.uri="/api/wifi",.method=HTTP_POST,.handler=wifi_post},
        {.uri="/api/wifi/random_password",.method=HTTP_GET,.handler=wifi_random},
        {.uri="/api/settings",.method=HTTP_GET,.handler=settings_get},
        {.uri="/api/settings",.method=HTTP_POST,.handler=settings_post},
        {.uri="/api/maint/exit",.method=HTTP_POST,.handler=reboot_device},
        {.uri="/api/factory",.method=HTTP_POST,.handler=factory_reset},
        {.uri="/api/reboot",.method=HTTP_POST,.handler=reboot_device},
        {.uri="/api/ota/check",.method=HTTP_POST,.handler=ota_check},
        {.uri="/api/ota",.method=HTTP_POST,.handler=ota_upload},
        {.uri="/api/ota/status",.method=HTTP_GET,.handler=ota_status},
    };
    for (unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);++i) {
        httpd_uri_t routed=routes[i];
        routed.handler=dispatch;routed.user_ctx=(void *)&routes[i];
        err=httpd_register_uri_handler(server,&routed);
        if (err!=ESP_OK) { app_maintenance_web_stop();return err; }
    }
    return ESP_OK;
}
esp_err_t app_maintenance_web_stop(void)
{
    /* Close admission first. SDK owns client descriptors and their closure;
     * never call raw transport operations from the application. Session close
     * is queued, so an active handler may wait for its configured I/O timeout.
     * Core's OTA shutdown callback aborts upload between receives. */
    atomic_store(&stopping,true);
    if (server) {
        int sessions[3];size_t count=3;
        if (httpd_get_client_list(server,&count,sessions)==ESP_OK)
            for (size_t i=0;i<count;++i) {
                /* A client may already be gone or the control queue may fail.
                 * Still join the server; stop failure retains the closed owner. */
                (void)httpd_sess_trigger_close(server,sessions[i]);
            }
    }
    esp_err_t err=server?httpd_stop(server):ESP_OK;
    if (err==ESP_OK) {
        server=NULL;
    }
    return err;
}
