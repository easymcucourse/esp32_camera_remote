#include "lcd_sim.h"
#include "atom_sim.h"
#include "atom_link.h"
#include "debug_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#if CONFIG_REMOTE_DBG_SIM
static SemaphoreHandle_t mutex;
static QueueHandle_t completed;
static atomic_bool enabled;
static atomic_uint epoch,outstanding;
static atom_sim_t sim;
static pad_player_t player={.state.battery=255};
typedef struct { uint32_t token,elapsed; bool cancelled; } completion_t;
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)xTaskGetTickCount()*portTICK_PERIOD_MS); }
static void apply(void *context,const pad_state_t *state) { (void)context;atom_sim_pad(&sim,state); }
static void done(void *context,uint32_t token,uint32_t elapsed,bool cancelled)
{
    (void)context;completion_t result={token,elapsed,cancelled};
    BaseType_t sent=xQueueSend(completed,&result,0);configASSERT(sent==pdTRUE);
}
static const pad_player_ops_t ops={apply,done,NULL};
static void task(void *context)
{
    (void)context;TickType_t cycle=xTaskGetTickCount();
    for (;;) {
        xSemaphoreTake(mutex,portMAX_DELAY);pad_player_tick(&player,now_ms(),&ops);xSemaphoreGive(mutex);
        vTaskDelayUntil(&cycle,pdMS_TO_TICKS(10));
    }
}
static void source_changed(void) { atomic_fetch_add(&epoch,1);atom_link_wake(); }
esp_err_t lcd_sim_start(void)
{
    atom_sim_init(&sim);mutex=xSemaphoreCreateMutex();completed=xQueueCreate(8,sizeof(completion_t));
    if (!mutex || !completed) return ESP_ERR_NO_MEM;
    return xTaskCreate(task,"lcd_pad_player",3072,NULL,3,NULL)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;
}
bool lcd_sim_enabled(void) { return atomic_load(&enabled); }
bool lcd_sim_online(void)
{ xSemaphoreTake(mutex,portMAX_DELAY);bool online=sim.online;xSemaphoreGive(mutex);return online; }
uint32_t lcd_sim_epoch(void) { return atomic_load(&epoch); }
void lcd_sim_poll(void)
{
    completion_t result;
    while (xQueueReceive(completed,&result,0)==pdTRUE) {
        debug_printf("[dbg] %s SIM pad token=%lu elapsed=%lums\n",result.cancelled?"FAIL":"DONE",
                     (unsigned long)result.token,(unsigned long)result.elapsed);
        atomic_fetch_sub(&outstanding,1);
    }
}
esp_err_t lcd_sim_transact(const uint8_t request[ATOM_REQUEST_SIZE],uint8_t reply[ATOM_RESPONSE_MAX],size_t *size)
{
    xSemaphoreTake(mutex,portMAX_DELAY);
    atom_sim_result_t result=atom_sim_transact(&sim,request,reply,size);
    xSemaphoreGive(mutex);
    return result==ATOM_SIM_OK?ESP_OK:result==ATOM_SIM_TIMEOUT?ESP_ERR_TIMEOUT:ESP_FAIL;
}
static bool number(const char *text,unsigned max,uint32_t *out)
{
    char *end;unsigned long n=strtoul(text,&end,10);
    if (!*text || *end || text[0]=='-' || n>max) return false;
    *out=n;return true;
}
bool lcd_sim_command(int argc,char **argv)
{
    bool atom=!strcmp(argv[0],"atom"),pad=!strcmp(argv[0],"pad"),gimbal=!strcmp(argv[0],"gimbal");
    if (!atom && !pad && !gimbal && strcmp(argv[0],"tap") && strcmp(argv[0],"hold") &&
        strcmp(argv[0],"release") && strcmp(argv[0],"stick") && strcmp(argv[0],"trigger") &&
        strcmp(argv[0],"shoot") && strcmp(argv[0],"record") && strcmp(argv[0],"seq")) return false;
    if (xSemaphoreTake(mutex,0)!=pdTRUE) { debug_printf("[dbg] ERR SIM player busy; retry\n");return true; }
    if (atom && argc==3 && !strcmp(argv[1],"sim") && (!strcmp(argv[2],"on") || !strcmp(argv[2],"off"))) {
        pad_player_cancel(&player,now_ms(),&ops);atom_sim_init(&sim);
        atomic_store(&enabled,!strcmp(argv[2],"on"));source_changed();
        debug_printf("[dbg] OK SIM atom sim %s requested RAM only\n",argv[2]);
    } else if (!lcd_sim_enabled()) debug_printf("[dbg] ERR SIM atom sim is off\n");
    else if (atom) {
        uint32_t n;
        if (argc==2 && (!strcmp(argv[1],"online") || !strcmp(argv[1],"offline"))) {
            if (!strcmp(argv[1],"offline")) { pad_player_cancel(&player,now_ms(),&ops);source_changed(); }
            sim.online=!strcmp(argv[1],"online");atom_link_wake();
            debug_printf("[dbg] OK SIM atom %s\n",argv[1]);
        } else if (argc==2 && !strcmp(argv[1],"reboot")) {
            pad_player_cancel(&player,now_ms(),&ops);atom_sim_reboot(&sim);atom_link_wake();
            debug_printf("[dbg] OK SIM atom reboot boot_id=%lu\n",(unsigned long)sim.boot_id);
        } else if (argc==3 && !strcmp(argv[1],"version") && number(argv[2],255,&n)) {
            sim.version=n;atom_link_wake();debug_printf("[dbg] OK SIM atom version=%lu\n",(unsigned long)n);
        } else if (argc==3 && (!strcmp(argv[1],"fail") || !strcmp(argv[1],"crc") || !strcmp(argv[1],"timeout")) && number(argv[2],10000,&n)) {
            uint32_t *field=!strcmp(argv[1],"fail")?&sim.fail:!strcmp(argv[1],"crc")?&sim.crc:&sim.timeout;
            *field=n;debug_printf("[dbg] OK SIM atom %s=%lu\n",argv[1],(unsigned long)n);
        } else debug_printf("[dbg] ERR SIM atom: sim on|off; online; offline; reboot; version 0..255; fail|crc|timeout 0..10000\n");
    } else if (gimbal) {
        unsigned n=argc==3 && !strcmp(argv[1],"state")?(!strcmp(argv[2],"off")?0:!strcmp(argv[2],"search")?1:!strcmp(argv[2],"connecting")?2:!strcmp(argv[2],"connected")?3:4):4;
        if (n>3) debug_printf("[dbg] ERR SIM gimbal state off|search|connecting|connected\n");
        else { sim.gimbal=n;debug_printf("[dbg] OK SIM gimbal=%u\n",n); }
    } else if (pad) {
        if (argc==2 && (!strcmp(argv[1],"connect") || !strcmp(argv[1],"disconnect"))) {
            pad_player_cancel(&player,now_ms(),&ops);player.state.connected=!strcmp(argv[1],"connect");apply(NULL,&player.state);
            debug_printf("[dbg] OK SIM pad %s\n",argv[1]);
        } else if (argc==3 && !strcmp(argv[1],"battery")) {
            uint32_t n;
            if (!strcmp(argv[2],"none")) n=255;
            else if (!number(argv[2],10,&n)) n=256;
            if (n>255) debug_printf("[dbg] ERR SIM battery 0..10|none\n");
            else { player.state.battery=n;apply(NULL,&player.state);debug_printf("[dbg] OK SIM battery=%lu\n",(unsigned long)n); }
        } else if (argc==2 && (!strcmp(argv[1],"gap") || !strcmp(argv[1],"overflow"))) {
            atom_sim_gap(&sim,!strcmp(argv[1],"overflow"));debug_printf("[dbg] OK SIM %s\n",argv[1]);
        } else debug_printf("[dbg] ERR SIM pad connect|disconnect; battery 0..10|none; gap; overflow\n");
    } else if (!player.state.connected || !sim.online) debug_printf("[dbg] ERR SIM pad or atom is disconnected\n");
    else {
        pad_sequence_t sequence;const char *error;
        if (!pad_cmd_parse(argc,argv,&sequence,&error)) debug_printf("[dbg] ERR SIM %s\n",error);
        else if (atomic_load(&outstanding)>=8 || player.count==4) debug_printf("[dbg] ERR SIM queue full\n");
        else {
            uint32_t next_token=debug_async_token();
            atomic_fetch_add(&outstanding,1);bool queued=pad_player_enqueue(&player,&sequence,next_token);configASSERT(queued);
            uint32_t budget=0;for (unsigned i=0;i<player.count;++i) budget+=player.jobs[(player.head+i)%4].sequence.duration_ms;
            if (sequence.camera_warning) debug_printf("[dbg] SIM WARN camera will receive shutter/record\n");
            debug_printf("[dbg] OK SIM queued token=%lu duration=%lums\n",(unsigned long)next_token,(unsigned long)budget);
        }
    }
    xSemaphoreGive(mutex);return true;
}
#else
esp_err_t lcd_sim_start(void) { return ESP_OK; }
bool lcd_sim_enabled(void) { return false; }
bool lcd_sim_online(void) { return false; }
uint32_t lcd_sim_epoch(void) { return 0; }
bool lcd_sim_command(int argc,char **argv) { (void)argc;(void)argv;return false; }
void lcd_sim_poll(void) {}
esp_err_t lcd_sim_transact(const uint8_t request[ATOM_REQUEST_SIZE],uint8_t reply[ATOM_RESPONSE_MAX],size_t *size)
{ (void)request;(void)reply;*size=0;return ESP_ERR_NOT_SUPPORTED; }
#endif
