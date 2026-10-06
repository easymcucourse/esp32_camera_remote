#include "ui_wifi_menu.h"
#include "ui_menu_messages.h"
#include "ui_model.h"
#include "app_console.h"
#include "network_config.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct fake_queue { unsigned size,head,count; unsigned char data[16*64]; };
static struct fake_queue *queue;
static TaskFunction_t worker;
static jmp_buf done;
static int64_t now;
static uint32_t input_epoch=2,wifi_epoch=7;
static unsigned phase,queues,deleted,resets,snapshots,prepares,commits,cancels,factories,queries;
static bool fail_queue,fail_task,fail_prepare,timeout_commit,reset_idle,slow_snapshot;
static unsigned random_calls,blocked_cancel;
static bool in_worker,run_on_wait,stop_in_send,retire_wifi,probe_start;
static app_message_type_t stop_rpc;
static void run(void) { assert(worker && !in_worker);in_worker=true;if(!setjmp(done))worker(NULL);in_worker=false; }
void vTaskDelete(void *task) { assert(!task && in_worker);worker=NULL;longjmp(done,1); }
void vTaskDelay(TickType_t ticks) { assert(ticks==10);now+=(int64_t)ticks*1000;if(run_on_wait && !in_worker && worker)run(); }
static network_config_t actual,next;
static uint32_t pending_token;
static unsigned pending_reads;
int64_t esp_timer_get_time(void) { return now; }
void app_ui_refresh_wifi_info(void) {}
void esp_fill_random(void *data,size_t size) { memset(data,0,size);++random_calls; }
const char *esp_err_to_name(esp_err_t e) { return e==ESP_OK ? "OK" : e==ESP_ERR_TIMEOUT ? "TIMEOUT" : "FAIL"; }
uint32_t app_console_endpoint_generation(app_endpoint_t e) { return e==APP_ENDPOINT_UI ? 5 : e==APP_ENDPOINT_WIFI ? wifi_epoch : e==APP_ENDPOINT_SYSTEM ? 8 : input_epoch; }
void app_message_release(app_message_t *m) { assert(!m->lease); }
QueueHandle_t xQueueCreate(unsigned capacity,size_t size)
{
    assert(capacity==16 && size<=64);if(fail_queue)return NULL;
    queue=calloc(1,sizeof(*queue));assert(queue);queue->size=(unsigned)size;++queues;return queue;
}
void vQueueDelete(QueueHandle_t q) { assert(q==queue);free(q);queue=NULL;--queues;++deleted; }
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t timeout)
{
    assert(q==queue && !timeout);if(stop_in_send){stop_in_send=false;assert(!ui_wifi_menu_quiesce(0) && queues==1);}
    if(q->count==16)return pdFALSE;
    memcpy(q->data+((q->head+q->count)%16)*q->size,item,q->size);++q->count;return pdTRUE;
}
BaseType_t xQueueReset(QueueHandle_t q) { assert(q==queue);q->head=q->count=0;++resets;reset_idle=true;return pdTRUE; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *ctx,unsigned priority,void *handle)
{
    assert(!strcmp(name,"wifi_menu") && stack==4096 && priority==2 && !ctx && !handle);
    if(probe_start) {
        app_message_t m={0};m.payload.action=(pad_action_t){PAD_ACTION_MENU_CONFIRM,0,0};
        assert(ui_wifi_menu_action(&m)==ESP_ERR_INVALID_STATE && !queue->count);
    }
    if(fail_task)return pdFALSE;
    worker=fn;return pdPASS;
}
esp_err_t app_console_send(app_message_t *m)
{ assert(m->type==APP_MESSAGE_WIFI_CONFIG_CANCEL && m->source==APP_ENDPOINT_UI && m->payload.command.token==pending_token);++cancels;if(blocked_cancel){--blocked_cancel;return ESP_ERR_TIMEOUT;}return ESP_OK; }
static void value(app_network_config_t *out)
{
    *out=(app_network_config_t){.channel=actual.channel,.show_password=actual.show_password,
        .default_password=network_config_uses_default_password(&actual)};
    strcpy(out->ssid,actual.ssid);strcpy(out->password,actual.password);
}
esp_err_t app_console_request(app_message_t *m,app_message_t *reply)
{
    assert(m->source==APP_ENDPOINT_UI && m->flags==APP_MESSAGE_REQUEST && m->generation==5 && !m->lease);
    if(m->deadline_us<=now)return ESP_ERR_TIMEOUT;
    *reply=(app_message_t){.result=ESP_OK};
    if(m->type==APP_MESSAGE_WIFI_STATUS) {
        ++snapshots;assert(m->target==APP_ENDPOINT_WIFI);value(&reply->payload.network.config);reply->payload.network.max_channel=11;
        if(slow_snapshot) { now+=600000;slow_snapshot=false; }
    }
    else if(m->type==APP_MESSAGE_WIFI_CONFIG_PREPARE) {
        assert(m->target==APP_ENDPOINT_WIFI);++prepares;
        if(fail_prepare){reply->result=ESP_FAIL;return ESP_OK;}
        next=(network_config_t){.channel=m->payload.config.channel,.show_password=m->payload.config.show_password};
        strcpy(next.ssid,m->payload.config.ssid);strcpy(next.password,m->payload.config.password);
        reply->payload.command.token=pending_token=100+prepares;pending_reads=0;
    } else if(m->type==APP_MESSAGE_WIFI_CONFIG_COMMIT) {
        assert(m->target==APP_ENDPOINT_WIFI && m->payload.command.token==pending_token && !m->payload.command.duration_ms);
        ++commits;actual=next;
        if(timeout_commit)return ESP_ERR_TIMEOUT;
    } else if(m->type==APP_MESSAGE_WIFI_CONFIG_RESULT) {
        assert(m->target==APP_ENDPOINT_WIFI && m->payload.command.token==pending_token);
        ++queries;reply->result=++pending_reads<3 ? ESP_ERR_NOT_FINISHED : ESP_OK;
        reply->payload.command.value=ESP_OK;
    } else assert(false);
    return ESP_OK;
}
esp_err_t app_console_request_cancelable(app_message_t *m,app_message_t *reply,app_console_cancel_fn cancelled,void *context)
{
    assert(cancelled && !context);
    if(cancelled(context)) return ESP_ERR_INVALID_STATE;
    esp_err_t result=app_console_request(m,reply);
    if(stop_rpc==m->type) { stop_rpc=0;assert(!ui_wifi_menu_quiesce(0) && queues==1);blocked_cancel=2;if(retire_wifi)++wifi_epoch; }
    return result;
}
static app_message_t action(pad_action_type_t type,int direction)
{
    app_message_t m={.type=APP_MESSAGE_UI_MENU_ACTION,.source=APP_ENDPOINT_INPUT,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=input_epoch,.endpoint_epoch=5,.deadline_us=now+500000};
    m.payload.action=(pad_action_t){type,direction,17};return m;
}
static void send(pad_action_type_t type,int direction)
{
    app_message_t m=action(type,direction),reply;
    if(type==PAD_ACTION_RELEASE_ALL) { m.flags=0;m.deadline_us=0; }
    assert(ui_menu_message_apply(&m,&reply)==ESP_OK);
}
static void script(void)
{
    const app_ui_wifi_menu_view_t *v=&ui_model_wifi_menu_view;
    switch(phase++) {
    case 0: send(PAD_ACTION_MENU_CONFIRM,0);assert(ui_wifi_menu_active());break;
    case 1: assert(v->active && v->selected==0 && strstr(v->lines[1],"DEFAULT"));send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 2: assert(strstr(v->footer,"Char 1"));send(PAD_ACTION_MENU_MOVE,1);break;
    case 3: send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 4: assert(strstr(v->lines[0],"SSID f") && actual.ssid[0]=='e');send(PAD_ACTION_MENU_MOVE,1);break;
    case 5: case 6: case 7: case 8: send(PAD_ACTION_MENU_MOVE,1);break;
    case 9: assert(v->selected==5);timeout_commit=true;send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 10: assert(prepares==1 && commits==1 && cancels==1 && strstr(v->lines[5],"APPLYING"));break;
    case 11: break;
    case 12: assert(strstr(v->footer,"Reconnect camera") && actual.ssid[0]=='f');send(PAD_ACTION_MENU_BACK,0);break;
    case 13: assert(!v->active && !ui_wifi_menu_active());send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 14: case 15: case 16: case 17: send(PAD_ACTION_MENU_MOVE,1);break;
    case 18: assert(v->selected==4);fail_prepare=true;timeout_commit=false;send(PAD_ACTION_MENU_STEP,1);break;
    case 19: assert(prepares==2 && commits==1 && actual.show_password && strstr(v->lines[4],"ON"));fail_prepare=false;send(PAD_ACTION_MENU_STEP,1);break;
    case 20: case 21: break;
    case 22: assert(!actual.show_password && strstr(v->footer,"Password display saved"));send(PAD_ACTION_MENU_BACK,0);break;
    case 23: send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 24: case 25: case 26: case 27: case 28: case 29: send(PAD_ACTION_MENU_MOVE,1);break;
    case 30: assert(v->selected==6 && !strcmp(v->lines[6],"BACK"));
        for(unsigned i=0;i<9;++i)assert(!strstr(v->lines[i],"RESET"));
        send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 31: assert(!v->active && !factories);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 32: assert(v->active);send(PAD_ACTION_RELEASE_ALL,0);break;
    case 33: assert(resets==1 && !v->active);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 34: assert(v->active);send(PAD_ACTION_RELEASE_ALL,0);break;
    case 35: assert(resets==2 && !ui_wifi_menu_active() && !v->active);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 36: {
        app_message_t m=action(PAD_ACTION_MENU_MOVE,1);
        for(unsigned i=0;i<16;++i)assert(ui_wifi_menu_action(&m)==ESP_OK);
        assert(ui_wifi_menu_action(&m)==ESP_ERR_NO_MEM);
        send(PAD_ACTION_RELEASE_ALL,0);
        assert(ui_wifi_menu_action(&m)==ESP_ERR_INVALID_STATE);break;
    }
    case 37: assert(resets==3 && !v->active);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 38: { app_message_t m=action(PAD_ACTION_MENU_MOVE,1);assert(ui_wifi_menu_action(&m)==ESP_OK);++input_epoch;break; }
    case 39: assert(!v->active);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 40: { app_message_t m=action(PAD_ACTION_MENU_MOVE,1);assert(ui_wifi_menu_action(&m)==ESP_OK);now+=500001;break; }
    case 41: assert(!v->active && factories==0 && prepares==3);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 42: send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 43: send(PAD_ACTION_MENU_MOVE,1);break;
    case 44: send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 45: case 46: case 47: case 48: case 49: send(PAD_ACTION_MENU_MOVE,1);break;
    case 50: actual.channel=10;send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 51: assert(strstr(v->footer,"Config changed; review draft") && prepares==3);slow_snapshot=true;send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 52: assert(strstr(v->footer,"TIMEOUT") && prepares==3 && commits==2);send(PAD_ACTION_MENU_BACK,0);break;
    case 53: assert(!v->active);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 54: case 55: send(PAD_ACTION_MENU_MOVE,1);break;
    case 56: assert(v->selected==2);send(PAD_ACTION_MENU_CONFIRM,0);break;
    case 57: assert(random_calls==12 && !strstr(v->lines[1],"DEFAULT"));atomic_fetch_add(&ui_model_connection_generation,1);break;
    case 58: assert(!v->active && !ui_wifi_menu_active());longjmp(done,1);
    default: assert(false);
    }
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t timeout)
{
    assert(q==queue && timeout==50);now+=50000;
    if(reset_idle) { reset_idle=false;return pdFALSE; }
    if(!q->count)script();
    if(!q->count)return pdFALSE;
    memcpy(item,q->data+q->head*q->size,q->size);q->head=(q->head+1)%16;--q->count;return pdTRUE;
}
int main(void)
{
    fail_queue=true;assert(ui_wifi_menu_start()==ESP_ERR_NO_MEM && !queues);fail_queue=false;
    fail_task=true;assert(ui_wifi_menu_start()==ESP_ERR_NO_MEM && !queues && deleted==1);fail_task=false;
    assert(ui_wifi_menu_start()==ESP_OK && queues==1);assert(ui_wifi_menu_start()==ESP_OK && queues==1);
    network_config_make_default(&actual);
    atomic_store(&ui_model_settings_mode,true);atomic_store(&ui_model_menu_selected,7);
    run();
    assert(phase==59 && snapshots>5 && queries==6);
    assert(!ui_wifi_menu_quiesce(0) && queues==1);
    assert(ui_wifi_menu_start()==ESP_ERR_INVALID_STATE);
    app_message_t m=action(PAD_ACTION_MENU_CONFIRM,0);
    assert(ui_wifi_menu_action(&m)==ESP_ERR_INVALID_STATE);
    run_on_wait=true;assert(ui_wifi_menu_quiesce(100) && queues==0 && !worker && !ui_wifi_menu_active());
    assert(ui_wifi_menu_quiesce(0));
    assert(ui_wifi_menu_start()==ESP_OK);stop_in_send=true;
    assert(ui_wifi_menu_action(&m)==ESP_OK);
    unsigned before=prepares;
    assert(ui_wifi_menu_quiesce(100) && prepares==before && queues==0);
    probe_start=true;assert(ui_wifi_menu_start()==ESP_OK);probe_start=false;
    phase=prepares=commits=cancels=0;now=0;timeout_commit=fail_prepare=false;reset_idle=false;
    network_config_make_default(&actual);atomic_store(&ui_model_settings_mode,true);atomic_store(&ui_model_menu_selected,7);
    stop_rpc=APP_MESSAGE_WIFI_CONFIG_PREPARE;run();
    assert(!worker && !commits && prepares==1 && cancels>=3);
    assert(ui_wifi_menu_quiesce(0) && queues==0 && !ui_wifi_menu_active());
    probe_start=true;assert(ui_wifi_menu_start()==ESP_OK);probe_start=false;
    phase=prepares=commits=cancels=0;now=0;timeout_commit=fail_prepare=false;reset_idle=false;
    network_config_make_default(&actual);atomic_store(&ui_model_settings_mode,true);atomic_store(&ui_model_menu_selected,7);
    retire_wifi=true;stop_rpc=APP_MESSAGE_WIFI_CONFIG_PREPARE;run();retire_wifi=false;blocked_cancel=0;
    assert(!worker && !commits && prepares==1 && !cancels);
    assert(ui_wifi_menu_quiesce(0) && queues==0);
    puts("UI Wi-Fi menu messages, commit timeout, persistence errors, retired reset rows and cancellation passed");
}

esp_err_t ui_mode_enter_normal(unsigned reason) { assert(reason<=APP_NORMAL_DISPLAY_TEST);return ESP_OK; }
