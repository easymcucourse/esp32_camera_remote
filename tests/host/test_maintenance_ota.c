#include "app_maintenance_ota.h"
#include "ota_header.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
#include "esp_heap_caps.h"
#include "cJSON.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char *scenario;
static unsigned prepared,cancelled,committed,admitted,released,begun,writes,ended,aborted,booted,allocated,freed;
static size_t written;
static bool shutdown_now;
static int context;
static unsigned char image[5000];
static const esp_partition_t running={"ota_0",6*1024*1024},target={"ota_1",6*1024*1024};
static const esp_app_desc_t description={.version="v1",.project_name="esp32_camera_remote",.date="Oct  3 2026",.time="11:50:00"};
static bool is(const char *name) { return !strcmp(scenario,name); }
static httpd_req_t request(void) { return (httpd_req_t){.content_len=sizeof(image),.body=image,.mime="application/octet-stream",.image_size="5000"}; }
static void check_context(void *arg) { assert(arg==&context); }
static bool prepare(void *arg) { check_context(arg);++prepared;return !is("reserve"); }
static void cancel(void *arg) { check_context(arg);++cancelled; }
static bool commit(void *arg,unsigned delay) { check_context(arg);assert(delay==1500);++committed;return true; }
static esp_err_t admit(void *arg) { check_context(arg);++admitted;return is("admission")?ESP_ERR_TIMEOUT:ESP_OK; }
static void release(void *arg) { check_context(arg);++released; }
static bool closing(void *arg) { check_context(arg);return shutdown_now; }
void core_boot_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "fake error"; }
int64_t esp_timer_get_time(void) { return 1000000; }
void *heap_caps_malloc(size_t bytes,unsigned caps)
{ assert(bytes==4096 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));++allocated;return is("allocation")?NULL:malloc(bytes); }
void heap_caps_free(void *data) { if(data)++freed;free(data); }
const esp_app_desc_t *esp_app_get_description(void) { return &description; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *partition) { assert(!partition);return &target; }
const esp_partition_t *esp_ota_get_running_partition(void) { return &running; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state)
{ *state=partition==&running?ESP_OTA_IMG_VALID:ESP_OTA_IMG_ABORTED;return ESP_OK; }
esp_err_t esp_ota_begin(const esp_partition_t *partition,size_t size,esp_ota_handle_t *handle)
{
    assert(partition==&target && size==OTA_WITH_SEQUENTIAL_WRITES);++begun;*handle=42;
    if(is("busy")) {
        httpd_req_t nested=request();unsigned old=prepared;
        assert(app_maintenance_ota_upload(&nested)==ESP_OK);
        assert(!strcmp(nested.status,"409 Conflict") && !nested.offset && prepared==old);
    }
    return is("begin")?ESP_FAIL:ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t handle,const void *data,size_t bytes)
{
    assert(handle==42 && data && bytes<=4096);++writes;
    assert(!memcmp(data,image+written,bytes));
    if((is("prefix_write") && writes==1)||(is("body_write") && writes==2))return ESP_FAIL;
    written+=bytes;if(is("interrupted") && writes==1)shutdown_now=true;return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t handle) { assert(handle==42 && written==sizeof(image));++ended;return is("end")?ESP_FAIL:ESP_OK; }
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *partition) { assert(partition==&target && ended==1);++booted;return is("set_boot")?ESP_FAIL:ESP_OK; }
esp_err_t esp_ota_abort(esp_ota_handle_t handle) { assert(handle==42);++aborted;return ESP_OK; }
int httpd_req_recv(httpd_req_t *req,char *data,size_t size)
{ if(size>17)size=17;assert(req->offset+size<=req->content_len);memcpy(data,req->body+req->offset,size);req->offset+=size;return (int)size; }
esp_err_t httpd_req_get_hdr_value_str(httpd_req_t *req,const char *key,char *value,size_t size)
{ const char *text=!strcmp(key,"Content-Type")?req->mime:req->image_size;if(!text || strlen(text)>=size)return ESP_FAIL;strcpy(value,text);return ESP_OK; }
esp_err_t httpd_resp_set_type(httpd_req_t *req,const char *type) { (void)req;assert(!strcmp(type,"application/json"));return ESP_OK; }
esp_err_t httpd_resp_set_hdr(httpd_req_t *req,const char *key,const char *value) { (void)req;assert(!strcmp(key,"Cache-Control") && !strcmp(value,"no-store"));return ESP_OK; }
esp_err_t httpd_resp_set_status(httpd_req_t *req,const char *status) { strcpy(req->status,status);return ESP_OK; }
esp_err_t httpd_resp_send(httpd_req_t *req,const char *text,int size)
{ assert(size==HTTPD_RESP_USE_STRLEN);snprintf(req->response,sizeof(req->response),"%s",text);return is("lost_reply") && strstr(text,"reboot_in_ms=")?ESP_FAIL:ESP_OK; }
esp_err_t httpd_resp_send_err(httpd_req_t *req,int code,const char *text) { (void)req;(void)text;assert(code==500);return ESP_OK; }
cJSON *cJSON_CreateObject(void) { return calloc(1,sizeof(cJSON)); }
void cJSON_Delete(cJSON *json) { free(json); }
char *cJSON_PrintUnformatted(const cJSON *json) { char *text=malloc(strlen(json->fields)+1);strcpy(text,json->fields);return text; }
void cJSON_AddStringToObject(cJSON *json,const char *key,const char *value)
{ size_t at=strlen(json->fields);snprintf(json->fields+at,sizeof(json->fields)-at,"%s=%s;",key,value); }
void cJSON_AddNumberToObject(cJSON *json,const char *key,double value) { char text[32];snprintf(text,sizeof(text),"%.0f",value);cJSON_AddStringToObject(json,key,text); }
void cJSON_AddBoolToObject(cJSON *json,const char *key,bool value) { cJSON_AddStringToObject(json,key,value?"true":"false"); }
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];
    image[0]=0xe9;image[1]=4;image[12]=9;image[23]=1;image[29]=1;
    image[32]=0x32;image[33]=0x54;image[34]=0xcd;image[35]=0xab;
    strcpy((char*)image+80,description.project_name);strcpy((char*)image+48,"v2");
    httpd_req_t req=request();assert(app_maintenance_ota_upload(&req)==ESP_OK && !strcmp(req.status,"409 Conflict"));
    assert(app_maintenance_ota_init(NULL)==ESP_ERR_INVALID_ARG);
    app_maintenance_ota_ops_t ops={prepare,cancel,commit,admit,release,closing,&context};
    app_maintenance_ota_ops_t invalid=ops;invalid.upload_end=NULL;
    assert(app_maintenance_ota_init(&invalid)==ESP_ERR_INVALID_ARG);
    assert(app_maintenance_ota_init(&ops)==ESP_OK && app_maintenance_ota_init(&ops)==ESP_ERR_INVALID_STATE);
    assert(!strcmp(app_maintenance_ota_boot_status(),"rolled_back"));
    req=request();req.content_len=288;
    assert(app_maintenance_ota_check(&req)==ESP_OK && strstr(req.response,"version=v2;") && !prepared);
    req=request();if(is("header"))image[12]=0;if(is("shutdown"))shutdown_now=true;
    esp_err_t result=app_maintenance_ota_upload(&req);
    bool success=is("success")||is("lost_reply")||is("busy");
    assert(result==(is("lost_reply")?ESP_FAIL:ESP_OK));
    if(success)assert(prepared==1 && admitted==1 && released==1 && committed==1 && !cancelled && !aborted && booted==1 && ended==1 && writes==3 && freed==1);
    else if(is("header")||is("shutdown"))assert(!prepared && !admitted && !allocated && !begun);
    else if(is("reserve"))assert(prepared==1 && !admitted && !cancelled && !allocated);
    else if(is("admission"))assert(prepared==1 && admitted==1 && cancelled==1 && !released && !allocated);
    else {
        assert(prepared==1 && admitted==1 && released==1 && cancelled==1 && !committed && allocated==1);
        assert(freed==(is("allocation")?0u:1u));
        assert(aborted==((is("prefix_write")||is("body_write")||is("interrupted"))?1u:0u));
    }
    req=request();app_maintenance_ota_status(&req);
    if(success)assert(strstr(req.response,"state=done;") && strstr(req.response,"received=5000;"));
    else if(released)assert(strstr(req.response,"state=failed;"));
    // A completed or failed upload releases the local mutex even while the
    // system admission remains closed; validate without a second flash write.
    shutdown_now=false;req=request();req.content_len=1;
    app_maintenance_ota_upload(&req);assert(!strcmp(req.status,"400 Bad Request"));
    return 0;
}
