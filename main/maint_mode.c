#include "maint_mode.h"
#include "maint_confirm.h"
#include "maint_notice.h"
#include "maint_ota.h"
#include "camera_pair.h"
#include "board_7b.h"
#include "debug_console.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
typedef struct { unsigned action;uint32_t token,epoch; } request_t;
typedef struct { uint32_t token;esp_err_t result;bool on,confirm; } result_t;
static QueueHandle_t requests,results;
static atomic_bool on,camera_session,close_for_camera;
static atomic_uint last_activity,outstanding;
static atomic_uint input_epoch;
static maint_confirm_t confirmation;
/* Owned exclusively by maint_ctl, including enable/disable. */
static maint_notice_t notice;
static atomic_bool paused;
static atomic_bool shutting_down,quiesced;
static atomic_bool uploading,upload_request,upload_reply;
static atomic_bool closing_service;
static atomic_int upload_error;
bool maint_mode_upload_active(void) { return atomic_load(&uploading); }
bool maint_mode_is_shutting_down(void) { return atomic_load(&shutting_down); }
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
bool maint_mode_is_on(void) { return atomic_load(&on); }
void maint_mode_touch(void) { atomic_store(&last_activity,now_ms()); }
void maint_mode_on_camera_session(bool open)
{
    atomic_store(&camera_session,open);
    if (open) atomic_store(&close_for_camera,true);
}
static esp_err_t disable(void)
{
    atomic_store(&closing_service,true);
    if (atomic_load(&uploading) && !atomic_load(&shutting_down)) {
        atomic_store(&closing_service,false);return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err=maint_web_stop();
    if (err!=ESP_OK) { atomic_store(&closing_service,false);return err; }
    atomic_store(&on,false);
    maint_notice_set(&notice,MAINT_NOTICE_NONE,now_ms());
    maint_confirm_cancel(&confirmation);board_7b_set_maint_menu(0);
    board_7b_set_maint_text("");
    board_7b_request_maint_screen(false);
    if (paused) { paused=false;camera_maintenance_release();if (!atomic_load(&shutting_down)) camera_jpeg_start(); }
    atomic_store(&closing_service,false);
    return ESP_OK;
}
static esp_err_t enable(bool stop)
{
    if (atomic_load(&on) || atomic_load(&shutting_down)) return ESP_ERR_INVALID_STATE;
    if (atomic_load(&camera_session) && !stop) return ESP_ERR_INVALID_STATE;
    if (stop) {
        if (!camera_maintenance_acquire(2000)) return ESP_ERR_TIMEOUT;
        paused=true;
    }
    if (atomic_load(&shutting_down)) {
        if (paused) { paused=false;camera_maintenance_release(); }
        return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&close_for_camera,false);
    esp_err_t err=maint_web_start();
    if (err==ESP_OK) {
        maint_notice_set(&notice,MAINT_NOTICE_NONE,now_ms());
        maint_mode_touch();atomic_store(&on,true);
        board_7b_set_maint_menu(3);
        if (atomic_load(&camera_session)) { disable();return ESP_ERR_INVALID_STATE; }
        board_7b_request_maint_screen(true);
    } else if (paused) { paused=false;camera_maintenance_release();if (!atomic_load(&shutting_down)) camera_jpeg_start(); }
    return err;
}
static void worker(void *arg)
{
    (void)arg;
    while (true) {
        if (atomic_load(&shutting_down)) {
            if (!atomic_load(&quiesced) && disable()==ESP_OK) atomic_store(&quiesced,true);
            vTaskDelay(pdMS_TO_TICKS(10));continue;
        }
        if (atomic_exchange(&upload_request,false)) {
            esp_err_t err=ESP_ERR_INVALID_STATE;
            if (atomic_load(&on) && atomic_load(&uploading) && !atomic_load(&shutting_down)) {
                err=paused?ESP_OK:camera_maintenance_acquire(2000)?ESP_OK:ESP_ERR_TIMEOUT;
                if (err==ESP_OK) paused=true;
            }
            if (err!=ESP_OK) atomic_store(&uploading,false);
            atomic_store(&upload_error,err);atomic_store(&upload_reply,true);
        }
        if (!board_7b_settings_mode() || board_7b_menu_selected()!=8) maint_confirm_cancel(&confirmation);
        maint_confirm_tick(&confirmation,now_ms(),atomic_load(&input_epoch));
        request_t request;
        if (xQueueReceive(requests,&request,pdMS_TO_TICKS(100))) {
            esp_err_t err=ESP_OK;
            if (request.action>=3) {
                if (request.epoch!=atomic_load(&input_epoch)) err=ESP_ERR_INVALID_STATE;
                else if (request.action==4) {
                    if (board_7b_settings_mode() || atomic_load(&camera_session)) err=ESP_ERR_INVALID_STATE;
                    else err=atomic_load(&on)?disable():enable(false);
                } else if (!board_7b_settings_mode() || board_7b_menu_selected()!=8) err=ESP_ERR_INVALID_STATE;
                else if (atomic_load(&on)) err=disable();
                else if (maint_confirm_press(&confirmation,now_ms(),request.epoch)) {
                    board_7b_set_maint_menu(2);err=enable(true);
                } else board_7b_set_maint_menu(1);
            } else { maint_confirm_cancel(&confirmation);err=request.action==0?disable():enable(request.action==2); }
            if (err!=ESP_OK) maint_notice_set(&notice,MAINT_NOTICE_ERROR,now_ms());
            result_t result={request.token,err,atomic_load(&on),confirmation.armed};
            xQueueSend(results,&result,0);
        }
        if (!atomic_load(&uploading) && atomic_exchange(&close_for_camera,false) && atomic_load(&on)) {
            if (disable()==ESP_OK) maint_notice_set(&notice,MAINT_NOTICE_CAMERA,now_ms());
        }
        bool showing_notice=maint_notice_tick(&notice,now_ms());
        board_7b_set_maint_menu(showing_notice && notice.kind==MAINT_NOTICE_ERROR?4:
                               atomic_load(&on)?3:confirmation.armed?1:0);
        if (!atomic_load(&on)) board_7b_set_maint_text(showing_notice && notice.kind==MAINT_NOTICE_CAMERA?
            "MAINTENANCE OFF: camera connected. Disconnect phone from Wi-Fi.":"");
        if (atomic_load(&on) && !atomic_load(&uploading)) {
            unsigned elapsed=now_ms()-atomic_load(&last_activity);
            if (elapsed>=600000) { disable();continue; }
            char pin[7],text[128];maint_web_pin(pin);
            unsigned seconds=(600000-elapsed+999)/1000;
            snprintf(text,sizeof(text),"MAINTENANCE http://192.168.4.1/ PIN %s %02u:%02u",pin,seconds/60,seconds%60);
            maint_ota_display(text,sizeof(text));
            board_7b_set_maint_text(text);
        }
    }
}
esp_err_t maint_mode_start(void)
{
    requests=xQueueCreate(4,sizeof(request_t));results=xQueueCreate(8,sizeof(result_t));
    if (!requests || !results) return ESP_ERR_NO_MEM;
    return xTaskCreate(worker,"maint_ctl",3072,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
static esp_err_t enqueue(unsigned action,uint32_t *token)
{
    if (atomic_load(&shutting_down)) return ESP_ERR_INVALID_STATE;
    unsigned count=atomic_load(&outstanding);
    do { if (count>=8 || !requests) return ESP_ERR_NO_MEM; }
    while (!atomic_compare_exchange_weak(&outstanding,&count,count+1));
    request_t request={action,debug_async_token(),atomic_load(&input_epoch)};
    if (!xQueueSend(requests,&request,0)) { atomic_fetch_sub(&outstanding,1);return ESP_ERR_NO_MEM; }
    if (token) *token=request.token;
    return ESP_OK;
}
esp_err_t maint_mode_request_off(void) { return enqueue(0,NULL); }
esp_err_t maint_mode_upload_begin(void)
{
    bool expected=false;
    if (!maint_mode_is_on() || maint_mode_is_shutting_down() ||
        !atomic_compare_exchange_strong(&uploading,&expected,true)) return ESP_ERR_INVALID_STATE;
    /* Close claims admission before testing uploading. This prevents the HTTP
     * handler waiting for an acknowledgement from a worker waiting in httpd_stop. */
    if (atomic_load(&closing_service) || !maint_mode_is_on() || maint_mode_is_shutting_down()) {
        atomic_store(&uploading,false);return ESP_ERR_INVALID_STATE;
    }
    atomic_store(&upload_reply,false);atomic_store(&upload_request,true);
    uint32_t started=now_ms();
    /* Worker acquisition is bounded to 2s; retain ownership until its acknowledgement. */
    while (!atomic_load(&upload_reply)) {
        if (maint_mode_is_shutting_down()) { atomic_store(&uploading,false);return ESP_ERR_INVALID_STATE; }
        if ((uint32_t)(now_ms()-started)>=5000) {
            atomic_store(&upload_request,false);atomic_store(&uploading,false);return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return atomic_load(&upload_error);
}
void maint_mode_upload_end(void)
{ maint_mode_touch();atomic_store(&close_for_camera,false);atomic_store(&uploading,false); }
bool maint_mode_quiesce(unsigned timeout)
{
    atomic_store(&shutting_down,true);
    if (!requests) return true;
    uint32_t started=now_ms();
    while (!atomic_load(&quiesced)) {
        if ((uint32_t)(now_ms()-started)>=timeout) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}
bool maint_mode_gamepad(pad_action_t action)
{
    if (action.type==PAD_ACTION_RELEASE_ALL || action.type==PAD_ACTION_UI_TOGGLE ||
        action.type==PAD_ACTION_MENU_MOVE || action.type==PAD_ACTION_MENU_BACK)
        atomic_fetch_add(&input_epoch,1);
    bool toggle=action.type==PAD_ACTION_MAINT_TOGGLE;
    bool menu=board_7b_settings_mode() && board_7b_menu_selected()==8;
    if (!toggle && !(menu && (action.type==PAD_ACTION_MENU_CONFIRM || action.type==PAD_ACTION_MENU_STEP))) return false;
    if (action.type==PAD_ACTION_MENU_STEP) return true; /* Repeat must never confirm STOP LIVE. */
    uint32_t token;esp_err_t err=enqueue(toggle?4:3,&token);
    if (err==ESP_OK) debug_printf("[dbg] maintenance pad queued token=%lu\n",(unsigned long)token);
    else debug_printf("[dbg] maintenance pad rejected %s\n",esp_err_to_name(err));
    return err==ESP_OK;
}
bool maint_mode_command(int argc,char **argv)
{
    if (strcmp(argv[0],"maint")) return false;
    if (argc==2 && !strcmp(argv[1],"status")) {
        char pin[7]="";if (atomic_load(&on)) maint_web_pin(pin);
        uint32_t elapsed=now_ms()-atomic_load(&last_activity);
        debug_printf("[dbg] OK maint on=%d paused=%d remaining_s=%lu pin=%s\n",atomic_load(&on),paused,
                     (unsigned long)(atomic_load(&on)&&elapsed<600000?(600000-elapsed)/1000:0),*pin?pin:"--");
        return true;
    }
    unsigned action=argc==2&&!strcmp(argv[1],"off")?0:
                    argc==2&&!strcmp(argv[1],"on")?1:
                    argc==3&&!strcmp(argv[1],"on")&&!strcmp(argv[2],"stop")?2:3;
    if (action==3) { debug_printf("[dbg] ERR usage: maint on [stop]|off|status\n");return true; }
    uint32_t token;esp_err_t err=enqueue(action,&token);
    if (err==ESP_OK) debug_printf("[dbg] OK maint queued token=%lu\n",(unsigned long)token);
    else debug_printf("[dbg] ERR maint %s\n",esp_err_to_name(err));
    return true;
}
void maint_mode_poll(void)
{
    result_t result;
    while (results && xQueueReceive(results,&result,0)) {
        atomic_fetch_sub(&outstanding,1);
        debug_printf("[dbg] %s maint token=%lu on=%d result=%s confirm=%d\n",result.result==ESP_OK?"DONE":"FAIL",
                     (unsigned long)result.token,result.on,esp_err_to_name(result.result),result.confirm);
    }
}
