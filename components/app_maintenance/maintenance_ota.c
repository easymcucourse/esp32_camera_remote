#include "app_maintenance_ota.h"
#include <stdatomic.h>
#include "ota_header.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(esp_image_header_t)==24 && sizeof(esp_app_desc_t)==256,"OTA prefix ABI");
_Static_assert(offsetof(esp_image_header_t,chip_id)==12 && offsetof(esp_app_desc_t,project_name)==48,"OTA offsets");
typedef struct { char state[16],error[80];unsigned received,total; } status_t;
static app_maintenance_ota_ops_t system_ops;
static bool initialized;
static atomic_bool upload_busy;
esp_err_t app_maintenance_ota_init(const app_maintenance_ota_ops_t *ops)
{
    if (!ops || !ops->restart_prepare || !ops->restart_cancel || !ops->restart_commit ||
        !ops->upload_begin || !ops->upload_end || !ops->shutting_down) return ESP_ERR_INVALID_ARG;
    if (initialized) return ESP_ERR_INVALID_STATE;
    system_ops=*ops;initialized=true;return ESP_OK;
}
static status_t status={.state="idle"};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static void progress(const char *state,unsigned received,unsigned total,const char *error)
{
    portENTER_CRITICAL(&mux);
    snprintf(status.state,sizeof(status.state),"%s",state);snprintf(status.error,sizeof(status.error),"%s",error?error:"");
    status.received=received;status.total=total;
    portEXIT_CRITICAL(&mux);
}
static esp_err_t reply(httpd_req_t *req,cJSON *json)
{
    char *text=cJSON_PrintUnformatted(json);cJSON_Delete(json);
    if (!text) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"out of memory");
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    esp_err_t err=httpd_resp_send(req,text,HTTPD_RESP_USE_STRLEN);free(text);return err;
}
static esp_err_t error_reply(httpd_req_t *req,const char *http,const char *error)
{ httpd_resp_set_status(req,http);cJSON *json=cJSON_CreateObject();cJSON_AddStringToObject(json,"error",error);return reply(req,json); }
static bool read_exact(httpd_req_t *req,uint8_t *buffer,size_t size)
{
    size_t at=0;
    while (at<size) {
        if (system_ops.shutting_down(system_ops.context)) return false;
        int got=httpd_req_recv(req,(char*)buffer+at,size-at);if (got<=0) return false;at+=got;
    }
    return true;
}
static const char *inspect(httpd_req_t *req,const uint8_t *prefix,size_t total,ota_header_info_t *info)
{
    char mime[48];
    if (httpd_req_get_hdr_value_str(req,"Content-Type",mime,sizeof(mime))!=ESP_OK || strcmp(mime,"application/octet-stream")) return "expected application/octet-stream";
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if (!target) return "no OTA partition";
    return ota_header_check(prefix,OTA_PREFIX_SIZE,total,target->size,ESP_CHIP_ID_ESP32S3,esp_app_get_description()->project_name,info);
}
esp_err_t app_maintenance_ota_check(httpd_req_t *req)
{
    if (!initialized || system_ops.shutting_down(system_ops.context)) return error_reply(req,"409 Conflict","maintenance unavailable");
    if (req->content_len!=OTA_PREFIX_SIZE) return error_reply(req,"400 Bad Request","expected 288 header bytes");
    char size_text[20];
    if (httpd_req_get_hdr_value_str(req,"X-Image-Size",size_text,sizeof(size_text))!=ESP_OK || !*size_text) return error_reply(req,"400 Bad Request","missing X-Image-Size");
    for (const char *p=size_text;*p;++p) if (*p<'0' || *p>'9') return error_reply(req,"400 Bad Request","invalid image size");
    unsigned long long total=strtoull(size_text,NULL,10);if (total>6*1024*1024) return error_reply(req,"413 Content Too Large","image too large");
    uint8_t prefix[OTA_PREFIX_SIZE];if (!read_exact(req,prefix,sizeof(prefix))) return error_reply(req,"408 Request Timeout","incomplete header");
    ota_header_info_t info;const char *error=inspect(req,prefix,(size_t)total,&info);
    if (error) return error_reply(req,"400 Bad Request",error);
    cJSON *json=cJSON_CreateObject();const esp_app_desc_t *current=esp_app_get_description();
    cJSON_AddStringToObject(json,"version",info.version);cJSON_AddStringToObject(json,"date",info.date);cJSON_AddStringToObject(json,"time",info.time);
    cJSON_AddStringToObject(json,"current_version",current->version);
    uint64_t incoming=ota_header_timestamp(info.date,info.time),running=ota_header_timestamp(current->date,current->time);
    cJSON_AddBoolToObject(json,"same_or_older",!strcmp(info.version,current->version)||(incoming && running && incoming<=running));
    return reply(req,json);
}
static esp_err_t upload(httpd_req_t *req)
{
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if (!target || req->content_len>target->size) return error_reply(req,"413 Content Too Large","image too large or no OTA partition");
    if (req->content_len<OTA_PREFIX_SIZE+32) return error_reply(req,"400 Bad Request","image too short");
    uint8_t prefix[OTA_PREFIX_SIZE];if (!read_exact(req,prefix,sizeof(prefix))) return error_reply(req,"408 Request Timeout","incomplete header");
    ota_header_info_t info;const char *error=inspect(req,prefix,req->content_len,&info);
    if (error) return error_reply(req,"400 Bad Request",error);
    if (!system_ops.restart_prepare(system_ops.context)) return error_reply(req,"409 Conflict","restart already pending");
    esp_err_t err=system_ops.upload_begin(system_ops.context);
    if (err!=ESP_OK) { system_ops.restart_cancel(system_ops.context);return error_reply(req,"409 Conflict",esp_err_to_name(err)); }
    unsigned received=0,total=req->content_len;esp_ota_handle_t handle=0;bool begun=false;
    uint8_t *chunk=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    progress("receiving",0,total,NULL);
    if (!chunk) { err=ESP_ERR_NO_MEM;goto failed; }
    err=esp_ota_begin(target,OTA_WITH_SEQUENTIAL_WRITES,&handle);if (err!=ESP_OK) goto failed;begun=true;
    err=esp_ota_write(handle,prefix,sizeof(prefix));if (err!=ESP_OK) goto failed;received=sizeof(prefix);
    while (received<total) {
        size_t size=total-received;if (size>4096) size=4096;
        if (!read_exact(req,chunk,size)) { err=ESP_ERR_TIMEOUT;goto failed; }
        err=esp_ota_write(handle,chunk,size);if (err!=ESP_OK) goto failed;
        received+=size;progress("receiving",received,total,NULL);
    }
    progress("verifying",received,total,NULL);
    err=esp_ota_end(handle);begun=false;if (err!=ESP_OK) goto failed;
    err=esp_ota_set_boot_partition(target);if (err!=ESP_OK) goto failed;
    heap_caps_free(chunk);progress("done",received,total,NULL);system_ops.upload_end(system_ops.context);
    cJSON *json=cJSON_CreateObject();cJSON_AddNumberToObject(json,"reboot_in_ms",1500);
    esp_err_t sent=reply(req,json);
    /* Image is validated and boot metadata committed. Reboot even if final acknowledgement is lost. */
    system_ops.restart_commit(system_ops.context,1500);return sent;
failed:
    if (begun) esp_ota_abort(handle);
    heap_caps_free(chunk);system_ops.restart_cancel(system_ops.context);progress("failed",received,total,esp_err_to_name(err));
    system_ops.upload_end(system_ops.context);ESP_LOGW("maint_ota","Upload failed: %s received=%u/%u",esp_err_to_name(err),received,total);
    return error_reply(req,"400 Bad Request",esp_err_to_name(err));
}
esp_err_t app_maintenance_ota_upload(httpd_req_t *req)
{
    bool expected=false;
    if (!initialized || system_ops.shutting_down(system_ops.context) ||
        !atomic_compare_exchange_strong(&upload_busy,&expected,true))
        return error_reply(req,"409 Conflict","upload unavailable");
    esp_err_t result=upload(req);
    atomic_store(&upload_busy,false);
    return result;
}
esp_err_t app_maintenance_ota_status(httpd_req_t *req)
{
    status_t copy;portENTER_CRITICAL(&mux);copy=status;portEXIT_CRITICAL(&mux);
    cJSON *json=cJSON_CreateObject();cJSON_AddStringToObject(json,"state",copy.state);
    cJSON_AddNumberToObject(json,"received",copy.received);cJSON_AddNumberToObject(json,"total",copy.total);cJSON_AddStringToObject(json,"error",copy.error);
    return reply(req,json);
}
const char *app_maintenance_ota_boot_status(void)
{
    esp_ota_img_states_t state;const esp_partition_t *run=esp_ota_get_running_partition();
    if (run && esp_ota_get_state_partition(run,&state)==ESP_OK) {
        if (state==ESP_OTA_IMG_PENDING_VERIFY) return "pending_verify";
        const esp_partition_t *other=esp_ota_get_next_update_partition(NULL);esp_ota_img_states_t other_state;
        if (other && esp_ota_get_state_partition(other,&other_state)==ESP_OK &&
            (other_state==ESP_OTA_IMG_ABORTED || other_state==ESP_OTA_IMG_INVALID)) return "rolled_back";
        if (state==ESP_OTA_IMG_VALID) return "valid";
    }
    return "unknown";
}
