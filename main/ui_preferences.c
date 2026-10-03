#include "ui_preferences.h"
#include "ui_overlay.h"
#include "atom_protocol.h"
#include "atom_link.h"
#include "board_7b.h"
#include "debug_console.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <stdatomic.h>
#include <string.h>
static const char *TAG="ui_preferences";
static QueueHandle_t requests,completed;
static atomic_uint level,outstanding,pad_type;
static atomic_bool resetting;
static SemaphoreHandle_t apply_lock;
typedef struct { uint32_t token; unsigned level; bool next; } request_t;
typedef struct { uint32_t token; unsigned level; esp_err_t error; } result_t;
unsigned ui_preferences_level(void) { return atomic_load(&level); }
static esp_err_t save(unsigned value)
{
    nvs_handle_t nvs;esp_err_t err=nvs_open("ui_prefs",NVS_READWRITE,&nvs);
    if (err!=ESP_OK) return err;
    err=nvs_set_u8(nvs,"info",value);
    if (err==ESP_OK) err=nvs_commit(nvs);
    nvs_close(nvs);return err;
}
unsigned ui_preferences_pad(void) { return atomic_load(&pad_type); }
esp_err_t ui_preferences_set_pad(unsigned mode)
{
    if (mode>ATOM_INPUT_XBOX || !apply_lock) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(apply_lock,portMAX_DELAY);
    esp_err_t err=ESP_ERR_INVALID_STATE;
    if (!atomic_load(&resetting)) {
        nvs_handle_t nvs;err=nvs_open("ui_prefs",NVS_READWRITE,&nvs);
        if (err==ESP_OK) {
            err=nvs_set_u8(nvs,"pad",mode);
            if (err==ESP_OK) err=nvs_commit(nvs);
            nvs_close(nvs);
        }
        if (err==ESP_OK) atomic_store(&pad_type,mode);
    }
    xSemaphoreGive(apply_lock);
    if (err==ESP_OK) atom_link_wake();
    return err;
}
static void worker(void *context)
{
    (void)context;request_t request;
    for (;;) if (xQueueReceive(requests,&request,portMAX_DELAY)==pdTRUE) {
        unsigned value=request.next?(ui_preferences_level()+1)%3:request.level;
        xSemaphoreTake(apply_lock,portMAX_DELAY);
        esp_err_t err=atomic_load(&resetting)?ESP_ERR_INVALID_STATE:save(value);
        if (err==ESP_OK) { atomic_store(&level,value);board_7b_set_info_level(value); }
        xSemaphoreGive(apply_lock);
        result_t result={request.token,ui_preferences_level(),err};
        BaseType_t sent=xQueueSend(completed,&result,0);configASSERT(sent==pdTRUE);
    }
}
esp_err_t ui_preferences_start(void)
{
    uint8_t value=0;nvs_handle_t nvs;esp_err_t err=nvs_open("ui_prefs",NVS_READONLY,&nvs);
    if (err==ESP_OK) { err=nvs_get_u8(nvs,"info",&value);nvs_close(nvs); }
    if (err!=ESP_OK || value>UI_INFO_HIDDEN) {
        value=UI_INFO_FULL;
        if (err!=ESP_ERR_NVS_NOT_FOUND) ESP_LOGW(TAG,"Invalid/unavailable preference (%s); full display",esp_err_to_name(err));
    }
    atomic_store(&level,value);board_7b_set_info_level(value);
    uint8_t mode=ATOM_INPUT_DS;
    if (nvs_open("ui_prefs",NVS_READONLY,&nvs)==ESP_OK) {
        if (nvs_get_u8(nvs,"pad",&mode)!=ESP_OK || mode>ATOM_INPUT_XBOX) mode=ATOM_INPUT_DS;
        nvs_close(nvs);
    }
    atomic_store(&pad_type,mode);
    requests=xQueueCreate(4,sizeof(request_t));completed=xQueueCreate(8,sizeof(result_t));apply_lock=xSemaphoreCreateMutex();
    if (!requests || !completed || !apply_lock) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG,"INFO %s loaded",ui_info_name(value));
    return xTaskCreate(worker,"ui_preferences",3072,NULL,2,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
esp_err_t ui_preferences_request(unsigned value,bool next,uint32_t *token)
{
    if (!requests || !token || (!next && value>UI_INFO_HIDDEN)) return ESP_ERR_INVALID_ARG;
    if (atomic_load(&resetting)) return ESP_ERR_INVALID_STATE;
    unsigned n=atomic_load(&outstanding);
    do { if (n>=8) return ESP_ERR_INVALID_STATE; } while (!atomic_compare_exchange_weak(&outstanding,&n,n+1));
    uint32_t id=debug_async_token();
    request_t request={id,value,next};
    if (xQueueSend(requests,&request,0)!=pdTRUE) { atomic_fetch_sub(&outstanding,1);return ESP_ERR_INVALID_STATE; }
    *token=id;return ESP_OK;
}
void ui_preferences_poll(void)
{
    result_t result;
    while (xQueueReceive(completed,&result,0)==pdTRUE) {
        debug_printf("[dbg] %s ui token=%lu info=%s result=%s\n",result.error==ESP_OK?"DONE":"FAIL",
            (unsigned long)result.token,ui_info_name(result.level),esp_err_to_name(result.error));
        atomic_fetch_sub(&outstanding,1);
    }
}
bool ui_preferences_command(int argc,char **argv)
{
    if (strcmp(argv[0],"ui")) return false;
    if (argc>=2 && !strcmp(argv[1],"pad")) {
        if (argc==2) debug_printf("[dbg] OK ui pad=%s\n",ui_preferences_pad()==ATOM_INPUT_DS?"ds":"xbox");
        else if (argc==3 && (!strcmp(argv[2],"ds") || !strcmp(argv[2],"xbox"))) {
            esp_err_t err=ui_preferences_set_pad(!strcmp(argv[2],"xbox"));
            debug_printf("[dbg] %s ui pad=%s result=%s\n",err==ESP_OK?"OK":"ERR",argv[2],esp_err_to_name(err));
        } else debug_printf("[dbg] ERR ui pad [ds|xbox]\n");
        return true;
    }
    if (argc==2 && !strcmp(argv[1],"info")) { debug_printf("[dbg] OK ui info=%s\n",ui_info_name(ui_preferences_level()));return true; }
    unsigned value=argc==3?!strcmp(argv[2],"full")?0:!strcmp(argv[2],"compact")?1:!strcmp(argv[2],"hidden")?2:3:3;
    bool next=argc==3 && !strcmp(argv[2],"next");
    if (argc!=3 || strcmp(argv[1],"info") || (value>2 && !next)) {
        debug_printf("[dbg] ERR ui info [full|compact|hidden|next]\n");return true;
    }
    uint32_t token;esp_err_t err=ui_preferences_request(value,next,&token);
    if (err!=ESP_OK) debug_printf("[dbg] ERR ui %s\n",esp_err_to_name(err));
    else debug_printf("[dbg] OK ui queued token=%lu\n",(unsigned long)token);
    return true;
}
esp_err_t ui_preferences_reset(void)
{
    atomic_store(&resetting,true);xSemaphoreTake(apply_lock,portMAX_DELAY);
    nvs_handle_t nvs;esp_err_t err=nvs_open("ui_prefs",NVS_READWRITE,&nvs);
    if (err==ESP_OK) {
        err=nvs_set_u8(nvs,"info",UI_INFO_FULL);
        if (err==ESP_OK) err=nvs_set_u8(nvs,"pad",ATOM_INPUT_DS);
        if (err==ESP_OK) err=nvs_commit(nvs);
        nvs_close(nvs);
    }
    if (err==ESP_OK) { atomic_store(&level,UI_INFO_FULL);atomic_store(&pad_type,ATOM_INPUT_DS);board_7b_set_info_level(UI_INFO_FULL); }
    else atomic_store(&resetting,false);
    xSemaphoreGive(apply_lock);return err;
}
