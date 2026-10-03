#include "maint_probe.h"
#include "sdkconfig.h"
#if CONFIG_REMOTE_DBG_SIM
#include "maint_mode.h"
#include "ui_preferences.h"
#include "debug_console.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "wifi_ap.h"
#include "app_restart.h"
#include "esp_ota_ops.h"
#include "esp_image_format.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "lwip/sockets.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static atomic_bool busy,done;
static uint32_t token;
static unsigned passed;
static bool success;
static bool wifi_restored;
static int exchange(const char *method,const char *path,const char *bearer,const char *body,char *response)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);if (fd<0) return -1;
    struct timeval timeout={.tv_sec=2};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    esp_netif_ip_info_t ip;
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (!ap || esp_netif_get_ip_info(ap,&ip)!=ESP_OK) { close(fd);return -1; }
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(80),.sin_addr.s_addr=ip.ip.addr};
    if (connect(fd,(struct sockaddr*)&address,sizeof(address))) { close(fd);return -1; }
    char header[384];int size=snprintf(header,sizeof(header),"%s %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: %u\r\n%s%s%s\r\n",method,path,(unsigned)(body?strlen(body):0),bearer?"Authorization: Bearer ":"",bearer?bearer:"",bearer?"\r\n":"");
    const char *parts[]={header,body?body:""};
    for (unsigned i=0;i<2;++i) {
        size_t at=0,length=i?strlen(parts[i]):(size_t)size;
        while (at<length) { int sent=send(fd,parts[i]+at,length-at,0);if (sent<=0) { close(fd);return -1; }at+=sent; }
    }
    size_t at=0;response[0]=0;
    while (at<8191) {
        int received=recv(fd,response+at,8191-at,0);
        if (received<=0) break;
        at+=received;response[at]=0;
        char *end=strstr(response,"\r\n\r\n"),*length=strstr(response,"Content-Length: ");
        if (end && length && at>=(size_t)(end-response)+4+strtoul(length+16,NULL,10)) break;
    }
    char *end=strstr(response,"\r\n\r\n"),*length=strstr(response,"Content-Length: ");
    if (!end || !length || at<(size_t)(end-response)+4+strtoul(length+16,NULL,10)) { close(fd);return -1; }
    close(fd);response[at]=0;int status=-1;sscanf(response,"HTTP/%*s %d",&status);return status;
}
static bool request(const char *method,const char *path,const char *bearer,const char *body,int expected,char *response)
{
    if (exchange(method,path,bearer,body,response)!=expected) return false;
    ++passed;return true;
}
static bool login_token(char *response,char output[33])
{
    char *body=strstr(response,"\r\n\r\n");if (!body) return false;
    cJSON *json=cJSON_Parse(body+4),*value=cJSON_GetObjectItemCaseSensitive(json,"token");
    bool ok=cJSON_IsString(value)&&strlen(value->valuestring)==32;
    if (ok) memcpy(output,value->valuestring,33);
    cJSON_Delete(json);return ok;
}
static cJSON *response_json(char *response)
{
    char *body=strstr(response,"\r\n\r\n");return body?cJSON_Parse(body+4):NULL;
}
static bool ota_binary(const char *path,const char *bearer,const esp_partition_t *run,
                       unsigned total,unsigned bytes,unsigned mutation,int expected,char *response)
{
    int fd=socket(AF_INET,SOCK_STREAM,0);if (fd<0) return false;
    struct timeval timeout={.tv_sec=10};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
    setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    esp_netif_ip_info_t ip;esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    uint8_t *chunk=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);bool ok=false;
    if (!chunk || !ap || esp_netif_get_ip_info(ap,&ip)!=ESP_OK) goto cleanup;
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(80),.sin_addr.s_addr=ip.ip.addr};
    if (connect(fd,(struct sockaddr*)&address,sizeof(address))) goto cleanup;
    char header[384];int length=snprintf(header,sizeof(header),"POST %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: application/octet-stream\r\nContent-Length: %u\r\nX-Image-Size: %u\r\nAuthorization: Bearer %s\r\n\r\n",path,bytes,total,bearer);
    size_t sent=0;while (sent<(size_t)length) { int n=send(fd,header+sent,length-sent,0);if (n<=0) goto cleanup;sent+=n; }
    for (unsigned offset=0;offset<bytes;) {
        unsigned size=bytes-offset;if (size>4096) size=4096;
        if (esp_partition_read(run,offset,chunk,size)!=ESP_OK) goto cleanup;
        if (!offset && mutation==1) chunk[12]=0;
        if (!offset && mutation==2) chunk[80]^=1;
        if (mutation==3 && offset+size==bytes) chunk[size-1]^=1;
        sent=0;while (sent<size) { int n=send(fd,chunk+sent,size-sent,0);if (n<=0) goto cleanup;sent+=n; }
        offset+=size;
    }
    size_t at=0;response[0]=0;
    while (at<8191) {
        int n=recv(fd,response+at,8191-at,0);if (n<=0) break;at+=n;response[at]=0;
        char *end=strstr(response,"\r\n\r\n"),*len=strstr(response,"Content-Length: ");
        if (end && len && at>=(size_t)(end-response)+4+strtoul(len+16,NULL,10)) break;
    }
    int status_code=0;char *end=strstr(response,"\r\n\r\n"),*len=strstr(response,"Content-Length: ");
    ok=end&&len&&at>=(size_t)(end-response)+4+strtoul(len+16,NULL,10)&&sscanf(response,"HTTP/%*s %d",&status_code)==1&&status_code==expected;
    if (ok) ++passed;
cleanup:
    heap_caps_free(chunk);close(fd);return ok;
}
static bool ota_audit(char *response,char *bearer)
{
    const esp_partition_t *run=esp_ota_get_running_partition(),*before=esp_ota_get_boot_partition();
    if (!run || !before) return false;
    esp_partition_pos_t pos={.offset=run->address,.size=run->size};esp_image_metadata_t metadata;
    if (esp_image_verify(ESP_IMAGE_VERIFY_SILENT,&pos,&metadata)!=ESP_OK) return false;
    unsigned total=metadata.image_len;
    if (!request("GET","/api/ota/status",NULL,NULL,401,response) || !request("GET","/api/ota/status",bearer,NULL,200,response)) return false;
    if (!ota_binary("/api/ota/check",bearer,run,total,288,1,400,response) ||
        !ota_binary("/api/ota/check",bearer,run,total,288,2,400,response) ||
        !ota_binary("/api/ota/check",bearer,run,total,288,0,200,response)) return false;
    if (!ota_binary("/api/ota",bearer,run,total,total,3,400,response)) return false;
    if (!request("GET","/api/ota/status",bearer,NULL,200,response) || !strstr(response,"failed") ||
        !maint_mode_is_on() || esp_ota_get_running_partition()!=run || esp_ota_get_boot_partition()!=before) return false;
    if (!ota_binary("/api/ota",bearer,run,total,total,0,200,response)) return false;
    cJSON *json=response_json(response),*delay=cJSON_GetObjectItemCaseSensitive(json,"reboot_in_ms");
    bool ok=cJSON_IsNumber(delay)&&delay->valueint==1500;cJSON_Delete(json);return ok;
}
static bool wifi_matches(char *response,const app_wifi_config_t *expected)
{
    cJSON *json=response_json(response),*ssid=cJSON_GetObjectItemCaseSensitive(json,"ssid"),
          *channel=cJSON_GetObjectItemCaseSensitive(json,"channel"),*length=cJSON_GetObjectItemCaseSensitive(json,"password_len");
    bool ok=cJSON_IsString(ssid)&&!strcmp(ssid->valuestring,expected->ssid)&&cJSON_IsNumber(channel)&&channel->valueint==expected->channel&&
            cJSON_IsNumber(length)&&length->valueint==(int)strlen(expected->password)&&!cJSON_HasObjectItem(json,"password");
    cJSON_Delete(json);return ok;
}
static uint32_t apply_token(char *response,unsigned expected_delay)
{
    cJSON *json=response_json(response),*value=cJSON_GetObjectItemCaseSensitive(json,"token"),
          *delay=cJSON_GetObjectItemCaseSensitive(json,"restart_in_ms");
    uint32_t number=cJSON_IsNumber(value)&&value->valuedouble>0&&value->valuedouble<=UINT32_MAX&&
        cJSON_IsNumber(delay)&&delay->valueint==(int)expected_delay?(uint32_t)value->valuedouble:0;
    cJSON_Delete(json);return number;
}
static char *wifi_body(const app_wifi_config_t *config)
{
    cJSON *json=cJSON_CreateObject();cJSON_AddStringToObject(json,"ssid",config->ssid);
    cJSON_AddStringToObject(json,"password",config->password);cJSON_AddNumberToObject(json,"channel",config->channel);
    char *body=cJSON_PrintUnformatted(json);cJSON_Delete(json);return body;
}
static bool wait_apply(uint32_t apply)
{
    for (unsigned i=0;i<140;++i) {
        esp_err_t result,state=wifi_ap_request_result(apply,&result);
        if (state==ESP_OK) return result==ESP_OK;
        if (state!=ESP_ERR_NOT_FINISHED) return false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}
static bool wifi_audit(char *response,char bearer[33],const char *login_body)
{
    app_wifi_config_t original,current,changed;wifi_ap_get_config(&original);changed=original;
    bool restore_needed=false,ok=false;char *body=NULL,pin_before[7],pin_after[7];maint_web_pin(pin_before);
    if (!request("GET","/api/wifi",NULL,NULL,401,response)) goto cleanup;
    if (!request("POST","/api/wifi",NULL,"{}",401,response)) goto cleanup;
    if (!request("GET","/api/wifi/random_password",NULL,NULL,401,response)) goto cleanup;
    if (!request("GET","/api/wifi",bearer,NULL,200,response)||!wifi_matches(response,&original)) goto cleanup;
    if (!request("GET","/api/wifi/random_password",bearer,NULL,200,response)) goto cleanup;
    cJSON *random=response_json(response),*password=cJSON_GetObjectItemCaseSensitive(random,"password");
    bool random_ok=cJSON_IsString(password)&&strlen(password->valuestring)==12&&wifi_config_check_password(password->valuestring)==WIFI_CFG_OK;
    cJSON_Delete(random);if (!random_ok) goto cleanup;
    const char *invalid[]={"{\"password\":\"1234567\"}","{\"ssid\":\"123456789012345678901234567890123\"}",
        "{\"channel\":0}","{\"channel\":1.5}","{\"channel\":\"1\"}","{\"other\":1}",
        "{\"ssid\":\"a\",\"ssid\":\"b\"}","{\"ssid\":\"prefix\\u0000suffix\"}","{\"ssid\":{}}","bad JSON"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i)
        if (!request("POST","/api/wifi",bearer,invalid[i],400,response)) goto cleanup;
    if (!request("POST","/api/wifi",bearer,"{}",200,response)) goto cleanup;
    cJSON *noop=response_json(response),*delay=cJSON_GetObjectItemCaseSensitive(noop,"restart_in_ms");
    bool noop_ok=cJSON_IsNumber(delay)&&delay->valueint==0;cJSON_Delete(noop);if (!noop_ok) goto cleanup;
    wifi_ap_get_config(&current);if (!wifi_config_equal(&current,&original)) goto cleanup;
    strcpy(changed.ssid,!strcmp(original.ssid,"web-wifi-test")?"web-wifi-test-2":"web-wifi-test");
    wifi_config_make_password(changed.password,esp_fill_random);
    changed.channel=original.channel<wifi_ap_max_channel()?original.channel+1:1;
    body=wifi_body(&changed);if (!body) goto cleanup;restore_needed=true;
    if (!request("POST","/api/wifi",bearer,body,200,response)) goto cleanup;
    uint32_t apply=apply_token(response,1500);free(body);body=NULL;if (!apply) goto cleanup;
    wifi_ap_get_config(&current);if (!wifi_config_equal(&current,&original)) goto cleanup;
    if (!wait_apply(apply)) goto cleanup;
    vTaskDelay(pdMS_TO_TICKS(250));wifi_ap_get_config(&current);maint_web_pin(pin_after);
    if (!maint_mode_is_on() || memcmp(pin_before,pin_after,7) || !wifi_config_equal(&current,&changed)) goto cleanup;
    if (!request("GET","/api/wifi",bearer,NULL,401,response)) goto cleanup;
    if (!request("POST","/api/login",NULL,login_body,200,response)||!login_token(response,bearer)) goto cleanup;
    if (!request("GET","/api/wifi",bearer,NULL,200,response)||!wifi_matches(response,&changed)) goto cleanup;
    body=wifi_body(&original);if (!body) goto cleanup;
    if (!request("POST","/api/wifi",bearer,body,200,response)) goto cleanup;
    apply=apply_token(response,1500);free(body);body=NULL;if (!apply || !wait_apply(apply)) goto cleanup;
    vTaskDelay(pdMS_TO_TICKS(250));wifi_ap_get_config(&current);maint_web_pin(pin_after);
    if (!wifi_config_equal(&current,&original) || memcmp(pin_before,pin_after,7)) goto cleanup;
    restore_needed=false;
    if (!request("GET","/api/wifi",bearer,NULL,401,response)) goto cleanup;
    if (!request("POST","/api/login",NULL,login_body,200,response)||!login_token(response,bearer)) goto cleanup;
    if (!request("GET","/api/wifi",bearer,NULL,200,response)||!wifi_matches(response,&original)) goto cleanup;
    ok=true;
cleanup:
    free(body);
    if (restore_needed) {
        uint32_t restore=0;
        for (unsigned i=0;i<10;++i) {
            if (wifi_ap_request_apply(&original,&restore)==ESP_OK) break;
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        if (restore) wait_apply(restore);
    }
    wifi_ap_get_config(&current);wifi_restored=wifi_config_equal(&current,&original);
    memset(original.password,0,sizeof(original.password));memset(changed.password,0,sizeof(changed.password));
    return ok&&wifi_restored;
}
static void worker(void *arg)
{
    passed=0;success=false;wifi_restored=true;
    char *response=heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    char pin[7],body[32],first[33],second[33];
    if (!response) goto finished;
    if (!request("GET","/",NULL,NULL,200,response) || !strstr(response,"Content-Encoding: gzip")) goto finished;
    if (!request("GET","/api/info",NULL,NULL,401,response)) goto finished;
    if (!request("POST","/api/login",NULL,"{\"pin\":\"wrong\"}",401,response)) goto finished;
    maint_web_pin(pin);snprintf(body,sizeof(body),"{\"pin\":\"%s\"}",pin);
    if (!request("POST","/api/login",NULL,body,200,response) || !login_token(response,first)) goto finished;
    if (!request("GET","/api/info",first,NULL,200,response) || !strstr(response,"running_partition")) goto finished;
    if (!request("POST","/api/login",NULL,body,200,response) || !login_token(response,second)) goto finished;
    if (!request("GET","/api/info",first,NULL,401,response)) goto finished;
    if (!request("GET","/api/info",second,NULL,200,response)) goto finished;
    if (!request("POST","/api/logout",second,"{}",200,response)) goto finished;
    if (!request("GET","/api/info",second,NULL,401,response)) goto finished;
    if (!request("POST","/api/login",NULL,"bad JSON",400,response)) goto finished;
    char oversized[514];memset(oversized,'x',513);oversized[513]=0;
    if (!request("POST","/api/login",NULL,oversized,413,response)) goto finished;
    if (!request("POST","/api/login",NULL,body,200,response) || !login_token(response,second)) goto finished;
    if ((uintptr_t)arg==4) {
        unsigned original=ui_preferences_pad();bool ok=true;
        if (!request("GET","/api/controller",NULL,NULL,401,response) ||
            !request("POST","/api/controller",NULL,"{\"type\":\"ds\"}",401,response)) ok=false;
        const char *invalid[]={"{}","{\"type\":1}","{\"type\":\"invalid\"}",
            "{\"type\":\"ds\",\"extra\":1}","{\"type\":\"ds\",\"type\":\"xbox\"}"};
        for (unsigned i=0;ok && i<5;++i)
            ok=request("POST","/api/controller",second,invalid[i],400,response);
        for (unsigned mode=0;ok && mode<2;++mode) {
            const char *body=mode?"{\"type\":\"xbox\"}":"{\"type\":\"ds\"}";
            ok=request("POST","/api/controller",second,body,200,response) &&
               ui_preferences_pad()==mode && request("GET","/api/controller",second,NULL,200,response);
            cJSON *reply=response_json(response),*type=cJSON_GetObjectItemCaseSensitive(reply,"type");
            ok=ok && cJSON_IsString(type) && !strcmp(type->valuestring,mode?"xbox":"ds");
            cJSON_Delete(reply);
        }
        success=ui_preferences_set_pad(original)==ESP_OK && ok;goto finished;
    }
    if ((uintptr_t)arg==3) { success=ota_audit(response,second);goto finished; }
    if ((uintptr_t)arg==2) {
        if (!request("POST","/api/reboot",NULL,"{\"confirm\":true}",401,response)) goto finished;
        const char *invalid[]={"{}","{\"confirm\":false}","{\"confirm\":1}",
            "{\"confirm\":true,\"other\":1}","{\"confirm\":true,\"confirm\":true}"};
        for (unsigned i=0;i<5;++i) if (!request("POST","/api/reboot",second,invalid[i],400,response)) goto finished;
        if (!request("POST","/api/reboot",second,"{\"confirm\":true}",200,response)) goto finished;
        cJSON *reply=response_json(response),*delay=cJSON_GetObjectItemCaseSensitive(reply,"reboot_in_ms");
        bool accepted=cJSON_IsNumber(delay)&&delay->valueint==1500;cJSON_Delete(reply);
        if (!accepted || app_restart_due() || !maint_mode_is_on()) goto finished;
        if (!request("POST","/api/reboot",second,"{\"confirm\":true}",409,response)) goto finished;
        success=true;goto finished;
    }
    if ((uintptr_t)arg==1 && !wifi_audit(response,second,body)) goto finished;
    if (!request("POST","/api/maint/exit",second,"{}",200,response)) goto finished;
    for (unsigned i=0;i<20 && maint_mode_is_on();++i) vTaskDelay(pdMS_TO_TICKS(100));
    if (maint_mode_is_on()) goto finished;
    success=true;
finished:
    heap_caps_free(response);memset(first,0,sizeof(first));memset(second,0,sizeof(second));
    atomic_store(&done,true);vTaskDeleteWithCaps(NULL);
}
bool maint_probe_command(int argc,char **argv)
{
    if ((argc!=2 && argc!=3) || strcmp(argv[0],"maint") || strcmp(argv[1],"probe")) return false;
    bool wifi=argc==3&&!strcmp(argv[2],"wifi");
    bool reboot=argc==3&&!strcmp(argv[2],"reboot");
    bool controller=argc==3&&!strcmp(argv[2],"controller");
    bool ota=argc==3&&!strcmp(argv[2],"ota");
    if (argc==3 && !wifi && !reboot && !ota && !controller) return false;
    bool expected=false;
    if (!maint_mode_is_on() || !atomic_compare_exchange_strong(&busy,&expected,true)) {
        debug_printf("[dbg] ERR maint probe requires active mode and idle probe\n");return true;
    }
    token=debug_async_token();
    if (xTaskCreateWithCaps(worker,"maint_probe",ota?8192:6144,(void*)(uintptr_t)(controller?4:ota?3:reboot?2:wifi?1:0),2,NULL,(ota?MALLOC_CAP_INTERNAL:MALLOC_CAP_SPIRAM)|MALLOC_CAP_8BIT)!=pdPASS) {
        atomic_store(&busy,false);debug_printf("[dbg] ERR maint probe memory\n");return true;
    }
    debug_printf("[dbg] OK maint probe queued token=%lu duration=%ums\n",(unsigned long)token,ota?90000:wifi?30000:10000);return true;
}
void maint_probe_poll(void)
{
    if (!atomic_exchange(&done,false)) return;
    debug_printf("[dbg] %s maint_probe token=%lu passed=%u transport=loopback restored=%d\n",success?"DONE":"FAIL",(unsigned long)token,passed,wifi_restored);
    atomic_store(&busy,false);
}
#else
bool maint_probe_command(int argc,char **argv) { (void)argc;(void)argv;return false; }
void maint_probe_poll(void) {}
#endif
