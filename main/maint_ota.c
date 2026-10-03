#include "maint_ota.h"
#include "ota_header.h"
#include "ota_health.h"
#include "maint_mode.h"
#include "app_restart.h"
#include "board_7b.h"
#include "camera_pair.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "cJSON.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
_Static_assert(sizeof(esp_image_header_t)==24 && sizeof(esp_app_desc_t)==256,"OTA prefix ABI");
_Static_assert(offsetof(esp_image_header_t,chip_id)==12 && offsetof(esp_app_desc_t,project_name)==48,"OTA offsets");
typedef struct { char state[16],error[80];unsigned received,total;uint32_t changed; } status_t;
static status_t status={.state="idle"};
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static bool ready,pending;
static int64_t ready_at,next_check;
static void progress(const char *state,unsigned received,unsigned total,const char *error)
{
    portENTER_CRITICAL(&mux);
    snprintf(status.state,sizeof(status.state),"%s",state);snprintf(status.error,sizeof(status.error),"%s",error?error:"");
    status.received=received;status.total=total;status.changed=(uint32_t)(esp_timer_get_time()/1000);
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
        if (maint_mode_is_shutting_down()) return false;
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
esp_err_t maint_ota_check(httpd_req_t *req)
{
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
esp_err_t maint_ota_upload(httpd_req_t *req)
{
    const esp_partition_t *target=esp_ota_get_next_update_partition(NULL);
    if (!target || req->content_len>target->size) return error_reply(req,"413 Content Too Large","image too large or no OTA partition");
    if (req->content_len<OTA_PREFIX_SIZE+32) return error_reply(req,"400 Bad Request","image too short");
    uint8_t prefix[OTA_PREFIX_SIZE];if (!read_exact(req,prefix,sizeof(prefix))) return error_reply(req,"408 Request Timeout","incomplete header");
    ota_header_info_t info;const char *error=inspect(req,prefix,req->content_len,&info);
    if (error) return error_reply(req,"400 Bad Request",error);
    if (!app_restart_prepare()) return error_reply(req,"409 Conflict","restart already pending");
    esp_err_t err=maint_mode_upload_begin();
    if (err!=ESP_OK) { app_restart_cancel();return error_reply(req,"409 Conflict",esp_err_to_name(err)); }
    unsigned received=0,total=req->content_len,last_percent=UINT32_MAX;esp_ota_handle_t handle=0;bool begun=false;
    uint8_t *chunk=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    progress("receiving",0,total,NULL);
    if (!chunk) { err=ESP_ERR_NO_MEM;goto failed; }
    err=esp_ota_begin(target,OTA_WITH_SEQUENTIAL_WRITES,&handle);if (err!=ESP_OK) goto failed;begun=true;
    err=esp_ota_write(handle,prefix,sizeof(prefix));if (err!=ESP_OK) goto failed;received=sizeof(prefix);
    while (received<total) {
        size_t size=total-received;if (size>4096) size=4096;
        if (!read_exact(req,chunk,size)) { err=ESP_ERR_TIMEOUT;goto failed; }
        err=esp_ota_write(handle,chunk,size);if (err!=ESP_OK) goto failed;
        received+=size;maint_mode_touch();progress("receiving",received,total,NULL);
        unsigned percent=(uint64_t)received*100/total;
        if (last_percent==UINT32_MAX || percent/5!=last_percent/5) {
            char text[40];snprintf(text,sizeof(text),"UPDATING %u%%",percent);board_7b_set_maint_text(text);last_percent=percent;
        }
    }
    progress("verifying",received,total,NULL);board_7b_set_maint_text("UPDATING verifying image...");
    err=esp_ota_end(handle);begun=false;if (err!=ESP_OK) goto failed;
    err=esp_ota_set_boot_partition(target);if (err!=ESP_OK) goto failed;
    heap_caps_free(chunk);progress("done",received,total,NULL);maint_mode_upload_end();
    cJSON *json=cJSON_CreateObject();cJSON_AddNumberToObject(json,"reboot_in_ms",1500);
    esp_err_t sent=reply(req,json);
    /* Image is validated and boot metadata committed. Reboot even if final acknowledgement is lost. */
    app_restart_commit(1500);return sent;
failed:
    if (begun) esp_ota_abort(handle);
    heap_caps_free(chunk);app_restart_cancel();progress("failed",received,total,esp_err_to_name(err));
    maint_mode_upload_end();ESP_LOGW("maint_ota","Upload failed: %s received=%u/%u",esp_err_to_name(err),received,total);
    return error_reply(req,"400 Bad Request",esp_err_to_name(err));
}
esp_err_t maint_ota_status(httpd_req_t *req)
{
    status_t copy;portENTER_CRITICAL(&mux);copy=status;portEXIT_CRITICAL(&mux);
    cJSON *json=cJSON_CreateObject();cJSON_AddStringToObject(json,"state",copy.state);
    cJSON_AddNumberToObject(json,"received",copy.received);cJSON_AddNumberToObject(json,"total",copy.total);cJSON_AddStringToObject(json,"error",copy.error);
    return reply(req,json);
}
bool maint_ota_display(char *text,size_t size)
{
    status_t copy;portENTER_CRITICAL(&mux);copy=status;portEXIT_CRITICAL(&mux);
    if (!strcmp(copy.state,"done")) { snprintf(text,size,"OTA COMPLETE - REBOOTING");return true; }
    if (!strcmp(copy.state,"failed") && (uint32_t)(esp_timer_get_time()/1000)-copy.changed<3000) {
        snprintf(text,size,"OTA FAILED: %s",copy.error);return true;
    }
    return false;
}
const char *maint_ota_boot_status(void)
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
void maint_ota_startup_ready(void)
{
    esp_ota_img_states_t state;const esp_partition_t *run=esp_ota_get_running_partition();
    pending=run && esp_ota_get_state_partition(run,&state)==ESP_OK && state==ESP_OTA_IMG_PENDING_VERIFY;
    ready=true;ready_at=esp_timer_get_time();
    next_check=0;
    ESP_LOGI("maint_ota","Boot running=%s status=%s; confirmation after 60s healthy runtime",run?run->label:"unknown",maint_ota_boot_status());
}
static void rollback(const char *reason)
{
    ESP_LOGE("maint_ota","OTA self-test failed (%s); requesting rollback",reason);
    if (maint_mode_quiesce(3000)) camera_maintenance_acquire(3000);
    esp_err_t err=esp_ota_mark_app_invalid_rollback_and_reboot();
    /* This API returns only on failure. Do not continue normal execution after
     * failed self-test; the bootloader re-evaluates the recorded slot states. */
    ESP_LOGE("maint_ota","Rollback API returned: %s; restarting for bootloader recovery",esp_err_to_name(err));
    esp_restart();
}
void maint_ota_health(void)
{
    int64_t now=esp_timer_get_time();if (!ready || !pending || now<next_check) return;next_check=now+1000000;
    if (board_7b_display_failed()) return; /* Normal fatal-display path closes maintenance before resetting; unconfirmed image rolls back. */
    bool heap_ok=heap_caps_check_integrity_all(true);
    esp_netif_t *ap=esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    ota_health_action_t action=ota_health_decide(now-ready_at,heap_ok,ap && esp_netif_is_netif_up(ap));
    if (action==OTA_HEALTH_ROLLBACK) {
        rollback(heap_ok?"hotspot unavailable after 60s":"heap integrity");
        return;
    }
    if (action==OTA_HEALTH_CONFIRM) {
        esp_err_t err=esp_ota_mark_app_valid_cancel_rollback();
        if (err==ESP_OK) { pending=false;ESP_LOGI("maint_ota","OTA confirmed after 60s healthy runtime"); }
        else { ESP_LOGE("maint_ota","OTA confirmation failed: %s",esp_err_to_name(err));rollback("confirmation failed"); }
    }
}
