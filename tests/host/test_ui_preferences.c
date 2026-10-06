#include "ui_preferences.h"
#include "app_console.h"
#include "async_token.h"
#include "app_wifi.h"
#include "nvs.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct fake_queue { unsigned cap,size,head,count; unsigned char *data; };
struct fake_semaphore { bool mutex,available; };
static TaskFunction_t worker;
static jmp_buf idle;
static bool in_worker,fail_task,fail_mutex,fail_commit;
static int fail_queue=-1,queue_index;
static unsigned queues,semaphores,commits,shown,replies,events;
static uint8_t stored_info=1,stored_pad=1,staged_info,staged_pad,stored_schema,staged_schema;
static int64_t now;
static uint32_t ui_epoch=5;
static uint32_t uart_epoch=3;
static unsigned blocked_events,delays;
static bool retire_uart,run_on_wait,stop_in_commit,stop_in_send,stop_in_delay;
static app_message_t last_reply;
static void run(void)
{
    assert(worker && !in_worker);in_worker=true;
    if (!setjmp(idle)) worker(NULL);
    in_worker=false;
}
QueueHandle_t xQueueCreate(unsigned cap,size_t size)
{
    if (queue_index++==fail_queue) return NULL;
    struct fake_queue *q=calloc(1,sizeof(*q));assert(q);q->cap=cap;q->size=(unsigned)size;
    assert(size<512);q->data=calloc(cap,size);assert(q->data);++queues;return q;
}
void vQueueDelete(QueueHandle_t q) { assert(q);free(q->data);free(q);--queues; }
BaseType_t xQueueSend(QueueHandle_t q,const void *item,TickType_t timeout)
{
    assert(q && !timeout);
    if(stop_in_send) { stop_in_send=false;assert(!ui_preferences_quiesce(0) && queues==2 && semaphores==1); }
    if(q->count==q->cap)return pdFALSE;
    memcpy(q->data+((q->head+q->count)%q->cap)*q->size,item,q->size);++q->count;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t q,void *item,TickType_t timeout)
{
    assert(q);
    if(!q->count) { if((timeout==portMAX_DELAY || timeout==pdMS_TO_TICKS(50)) && in_worker)longjmp(idle,1);return pdFALSE; }
    memcpy(item,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->cap;--q->count;return pdTRUE;
}
unsigned uxQueueMessagesWaiting(QueueHandle_t q) { assert(q);return q->count; }
void vTaskDelete(void *task) { assert(!task && in_worker);worker=NULL;longjmp(idle,1); }
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    if(fail_mutex)return NULL;
    struct fake_semaphore *s=calloc(1,sizeof(*s));assert(s);s->mutex=true;s->available=true;++semaphores;return s;
}
SemaphoreHandle_t xSemaphoreCreateBinary(void)
{ struct fake_semaphore *s=calloc(1,sizeof(*s));assert(s);++semaphores;return s; }
void vSemaphoreDelete(SemaphoreHandle_t s) { assert(s);free(s);--semaphores; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t timeout)
{
    assert(s && timeout==portMAX_DELAY);
    if(!s->available && !s->mutex)run();
    assert(s->available);s->available=false;return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(s && !s->available);s->available=true;return pdTRUE; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{
    assert(!strcmp(name,"ui_preferences") && stack==3072 && priority==2 && !context && !handle);
    if(fail_task)return pdFALSE;
    worker=fn;return pdPASS;
}
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "fake"; }
void app_ui_set_info_level(unsigned value) { shown=value; }
int64_t esp_timer_get_time(void) { return now; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ assert(endpoint==APP_ENDPOINT_UI || endpoint==APP_ENDPOINT_UART);return endpoint==APP_ENDPOINT_UI ? ui_epoch : uart_epoch; }
void vTaskDelay(TickType_t ticks)
{ assert(ticks==pdMS_TO_TICKS(10));++delays;now+=(int64_t)ticks*1000;
  if (in_worker && stop_in_delay) { stop_in_delay=false;assert(!ui_preferences_quiesce(0)); }
  if (in_worker && retire_uart) ++uart_epoch;
  if (!in_worker && run_on_wait && worker) run(); }
esp_err_t app_console_reply(const app_message_t *origin,app_message_t *reply)
{ assert(in_worker && origin->flags==APP_MESSAGE_REQUEST);last_reply=*reply;++replies;return ESP_OK; }
esp_err_t app_console_send(app_message_t *message)
{ assert(in_worker && message->type==APP_MESSAGE_UI_PREFERENCES && message->flags==APP_MESSAGE_EVENT);if (blocked_events) { --blocked_events;return ESP_ERR_TIMEOUT; }last_reply=*message;++events;return ESP_OK; }
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *out)
{ assert(!strcmp(name,"ui_prefs"));if(mode==NVS_READWRITE){assert(in_worker);staged_info=stored_info;staged_pad=stored_pad;staged_schema=stored_schema;}*out=1;return ESP_OK; }
esp_err_t nvs_get_u8(nvs_handle_t nvs,const char *key,uint8_t *value)
{ assert(nvs==1 && !in_worker);if(!strcmp(key,"schema")){if(!stored_schema)return ESP_ERR_NVS_NOT_FOUND;*value=stored_schema;return ESP_OK;}*value=!strcmp(key,"info") ? stored_info : stored_pad;return ESP_OK; }
esp_err_t nvs_set_u8(nvs_handle_t nvs,const char *key,uint8_t value)
{ assert(nvs==1 && in_worker);if(!strcmp(key,"schema"))staged_schema=value;else if(!strcmp(key,"info"))staged_info=value;else{assert(!strcmp(key,"pad"));staged_pad=value;}return ESP_OK; }
esp_err_t nvs_commit(nvs_handle_t nvs)
{ assert(nvs==1 && in_worker);++commits;if(stop_in_commit){stop_in_commit=false;assert(!ui_preferences_quiesce(0) && queues==2 && semaphores==2);}
if(fail_commit) { return ESP_FAIL; } stored_info=staged_info;stored_pad=staged_pad;stored_schema=staged_schema;return ESP_OK; }
void nvs_close(nvs_handle_t nvs) { assert(nvs==1); }
static app_message_t message(unsigned op,unsigned value)
{
    app_message_t m={.type=APP_MESSAGE_UI_PREFERENCES,.source=APP_ENDPOINT_INPUT,
        .target=APP_ENDPOINT_UI,.flags=APP_MESSAGE_REQUEST,.generation=3,.endpoint_epoch=ui_epoch,.deadline_us=1000000};
    m.payload.command.index=op;m.payload.command.value=value;return m;
}
int main(void)
{
    for(int failure=0;failure<4;++failure) {
        queue_index=0;fail_queue=failure<2 ? failure : -1;fail_mutex=failure==2;fail_task=failure==3;
        assert(ui_preferences_start()==ESP_ERR_NO_MEM && queues==0 && semaphores==0);
    }
    fail_queue=-1;fail_mutex=fail_task=false;queue_index=0;
    assert(ui_preferences_start()==ESP_OK && queues==2 && semaphores==1);
    assert(ui_preferences_start()==ESP_OK && queues==2 && semaphores==1);
    assert(shown==1 && ui_preferences_level()==1 && ui_preferences_pad()==1);
    uint32_t first=async_token_next(),token;unsigned value;esp_err_t error;
    uint32_t wifi=app_wifi_next_token();assert(wifi>first);first=wifi;
    assert(ui_preferences_request(0,true,&token)==ESP_OK && token>first);
    assert(ui_preferences_request(0,true,&first)==ESP_OK && first>token);
    run();assert(shown==0 && stored_info==0);
    assert(ui_preferences_result(&token,&value,&error) && value==2 && error==ESP_OK);
    assert(ui_preferences_result(&token,&value,&error) && value==0 && error==ESP_OK);
    assert(!ui_preferences_result(&token,&value,&error));
    for(unsigned i=0;i<4;++i)assert(ui_preferences_request(i%3,false,&token)==ESP_OK);
    assert(ui_preferences_request(0,false,&token)==ESP_ERR_INVALID_STATE);run();
    while(ui_preferences_result(&token,&value,&error)){}
    assert(ui_preferences_set_pad(0)==ESP_OK && stored_pad==0 && ui_preferences_pad()==0 && semaphores==1);
    fail_commit=true;assert(ui_preferences_set_pad(1)==ESP_FAIL && stored_pad==0 && ui_preferences_pad()==0);fail_commit=false;

    app_message_t m=message(APP_UI_PREF_INFO_SET,2),reply={0};bool deferred;
    unsigned old=commits;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred && commits==old);
    run();assert(replies==1 && last_reply.result==ESP_OK && stored_info==2 && shown==2);
    m=message(APP_UI_PREF_GET,0);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && !deferred);
    assert(reply.payload.command.value==2 && reply.payload.command.direction==0);
    m=message(APP_UI_PREF_PAD_SET,1);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);
    run();assert(last_reply.result==ESP_OK && stored_pad==1);
    m=message(APP_UI_PREF_INFO_NEXT,0);m.flags=0;assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);
    run();assert(events==1 && shown==0 && last_reply.payload.command.value==0);
    m=message(APP_UI_PREF_INFO_SET,1);old=commits;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);now=1000001;run();
    assert(last_reply.result==ESP_ERR_TIMEOUT && commits==old);now=0;
    m=message(APP_UI_PREF_INFO_SET,1);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);
    ++ui_epoch;run();assert(last_reply.result==ESP_ERR_INVALID_STATE && commits==old);
    m=message(APP_UI_PREF_INFO_NEXT,0);m.source=APP_ENDPOINT_UART;m.payload.command.flag=true;
    unsigned prior_replies=replies,prior_events=events;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && !deferred && reply.payload.command.token);
    uint32_t admitted_token=reply.payload.command.token;blocked_events=2;run();
    assert(delays==2 && replies==prior_replies && events==prior_events+1 && last_reply.payload.command.token==admitted_token);
    m=message(APP_UI_PREF_INFO_NEXT,0);m.source=APP_ENDPOINT_UART;m.payload.command.flag=true;
    old=commits;assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && !deferred);
    ++uart_epoch;run();assert(commits==old && events==prior_events+1);
    m=message(APP_UI_PREF_INFO_NEXT,0);m.source=APP_ENDPOINT_UART;m.generation=uart_epoch;m.payload.command.flag=true;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && !deferred);
    retire_uart=true;blocked_events=1;run();retire_uart=false;
    assert(events==prior_events+1 && delays==3);
    m=message(APP_UI_PREF_GET,0);m.source=APP_ENDPOINT_UART;m.payload.command.flag=true;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    m=message(APP_UI_PREF_INFO_SET,0);m.payload.command.flag=true;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    m=message(APP_UI_PREF_RESET,0);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    m.source=APP_ENDPOINT_SYSTEM;fail_commit=true;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);
    assert(ui_preferences_request(0,true,&token)==ESP_ERR_INVALID_STATE);
    run();assert(last_reply.result==ESP_FAIL && stored_pad==1);fail_commit=false;
    assert(ui_preferences_request(0,true,&token)==ESP_OK);run();while(ui_preferences_result(&token,&value,&error)){}
    m=message(APP_UI_PREF_RESET,0);m.source=APP_ENDPOINT_SYSTEM;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);run();
    assert(last_reply.result==ESP_OK && stored_info==0 && stored_pad==0 && shown==0);
    assert(ui_preferences_set_pad(1)==ESP_ERR_INVALID_STATE && semaphores==1);
    assert(ui_preferences_request(0,true,&token)==ESP_ERR_INVALID_STATE);
    m=message(APP_UI_PREF_INFO_SET,3);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    m=message(APP_UI_PREF_PAD_SET,2);assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    m=message(APP_UI_PREF_GET,0);m.source=APP_ENDPOINT_CAMERA;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_ERR_INVALID_ARG);
    assert(!ui_preferences_quiesce(0) && queues==2 && semaphores==1);
    assert(ui_preferences_start()==ESP_ERR_INVALID_STATE);
    assert(ui_preferences_request(0,true,&token)==ESP_ERR_INVALID_STATE);
    run_on_wait=true;assert(ui_preferences_quiesce(100) && queues==0 && semaphores==0 && !worker);
    assert(ui_preferences_quiesce(0));
    assert(ui_preferences_start()==ESP_OK);
    old=commits;
    assert(ui_preferences_request(2,false,&token)==ESP_OK);
    assert(!ui_preferences_quiesce(0));
    assert(ui_preferences_quiesce(100) && commits==old && queues==0 && semaphores==0);
    assert(ui_preferences_start()==ESP_OK);
    stop_in_commit=true;
    assert(ui_preferences_set_pad(1)==ESP_OK && stored_pad==1);
    assert(ui_preferences_quiesce(100) && queues==0 && semaphores==0);
    assert(ui_preferences_start()==ESP_OK);
    assert(ui_preferences_request(1,false,&token)==ESP_OK);run();
    assert(ui_preferences_quiesce(100) && queues==0 && semaphores==0);
    assert(ui_preferences_start()==ESP_OK);old=commits;stop_in_send=true;
    assert(ui_preferences_request(2,false,&token)==ESP_OK);
    assert(ui_preferences_quiesce(100) && commits==old && queues==0 && semaphores==0);
    assert(ui_preferences_start()==ESP_OK);old=commits;
    m=message(APP_UI_PREF_INFO_SET,2);
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && deferred);
    assert(!ui_preferences_quiesce(0));
    assert(ui_preferences_quiesce(100) && commits==old && last_reply.result==ESP_ERR_INVALID_STATE);
    assert(ui_preferences_start()==ESP_OK);
    m=message(APP_UI_PREF_INFO_NEXT,0);m.source=APP_ENDPOINT_UART;m.generation=uart_epoch;m.payload.command.flag=true;
    assert(ui_preferences_message(&m,&reply,&deferred)==ESP_OK && !deferred);
    blocked_events=10;stop_in_delay=true;run();
    assert(ui_preferences_quiesce(0) && queues==0 && semaphores==0 && !worker);
    blocked_events=0;
    stored_info=2;stored_pad=1;stored_schema=2;
    assert(ui_preferences_start()==ESP_OK && ui_preferences_level()==0 && ui_preferences_pad()==0);
    assert(ui_preferences_quiesce(100) && queues==0 && semaphores==0);
    puts("UI preferences internal worker, ordered next, errors, deferred replies and reset passed");
}
