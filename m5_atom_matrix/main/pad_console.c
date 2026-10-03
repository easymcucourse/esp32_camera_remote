#include "pad_console.h"
#include "pad_player.h"
#include "ds4_host.h"
#include "debug_console.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

#if CONFIG_REMOTE_DBG_SIM
static pad_player_t player={.state.battery=255};
static SemaphoreHandle_t mutex;
static QueueHandle_t completed;
static atomic_uint outstanding;
typedef struct { uint32_t token, elapsed; bool cancelled; } completion_t;
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)xTaskGetTickCount()*portTICK_PERIOD_MS); }
static void apply(void *context, const pad_state_t *state)
{ (void)context; ds4_host_apply_sim(state); }
static void done(void *context, uint32_t token, uint32_t elapsed, bool cancelled)
{
    (void)context; completion_t result={token,elapsed,cancelled};
    /* Outstanding reservations include jobs and results, so this cannot overflow. */
    BaseType_t sent=xQueueSend(completed,&result,0); configASSERT(sent==pdTRUE);
}
static const pad_player_ops_t ops={apply,done,NULL};
static void player_task(void *arg)
{
    (void)arg; TickType_t cycle=xTaskGetTickCount();
    for (;;) {
        xSemaphoreTake(mutex,portMAX_DELAY);
        pad_player_tick(&player,now_ms(),&ops);
        xSemaphoreGive(mutex);
        vTaskDelayUntil(&cycle,pdMS_TO_TICKS(10));
    }
}
void pad_console_poll(void)
{
    completion_t result;
    while (xQueueReceive(completed,&result,0)==pdTRUE) {
        debug_printf("[dbg] %s SIM pad token=%lu elapsed=%lums\n",result.cancelled?"FAIL":"DONE",
                     (unsigned long)result.token,(unsigned long)result.elapsed);
        atomic_fetch_sub(&outstanding,1);
    }
}
esp_err_t pad_console_start(void)
{
    mutex=xSemaphoreCreateMutex(); completed=xQueueCreate(8,sizeof(completion_t));
    if (!mutex || !completed) return ESP_ERR_NO_MEM;
    if (xTaskCreate(player_task,"pad_player",3072,NULL,3,NULL)!=pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
bool pad_console_command(int argc, char **argv)
{
    bool pad=!strcmp(argv[0],"pad");
    if (!pad && strcmp(argv[0],"tap") && strcmp(argv[0],"hold") && strcmp(argv[0],"release") &&
        strcmp(argv[0],"stick") && strcmp(argv[0],"trigger") && strcmp(argv[0],"shoot") &&
        strcmp(argv[0],"record") && strcmp(argv[0],"seq")) return false;
    if (xSemaphoreTake(mutex,0)!=pdTRUE) { debug_printf("[dbg] ERR SIM player busy; retry\n"); return true; }
    if (pad && argc==3 && !strcmp(argv[1],"sim") && (!strcmp(argv[2],"on") || !strcmp(argv[2],"off"))) {
        pad_player_cancel(&player,now_ms(),&ops);
        ds4_host_set_sim(!strcmp(argv[2],"on"));
        debug_printf("[dbg] OK SIM pad sim %s\n",argv[2]);
    } else if (!ds4_host_sim_active()) debug_printf("[dbg] ERR SIM pad sim is off\n");
    else if (pad) {
        if (argc==2 && (!strcmp(argv[1],"connect") || !strcmp(argv[1],"disconnect"))) {
            pad_player_cancel(&player,now_ms(),&ops);
            player.state.connected=!strcmp(argv[1],"connect"); apply(NULL,&player.state);
            debug_printf("[dbg] OK SIM pad %s\n",argv[1]);
        } else if (argc==3 && !strcmp(argv[1],"battery")) {
            char *end; long n=strtol(argv[2],&end,10);
            if (!strcmp(argv[2],"none")) n=255;
            else if (!*argv[2] || *end || n<0 || n>10) n=-1;
            if (n<0) debug_printf("[dbg] ERR SIM battery must be 0..10 or none\n");
            else { player.state.battery=(uint8_t)n; apply(NULL,&player.state); debug_printf("[dbg] OK SIM battery=%ld\n",n); }
        } else if (argc==2 && !strcmp(argv[1],"overflow")) {
            ds4_host_sim_overflow(); debug_printf("[dbg] OK SIM overflow\n");
        } else debug_printf("[dbg] ERR SIM usage: pad sim on|off; pad connect|disconnect; pad battery 0..10|none; pad overflow\n");
    } else if (!player.state.connected) debug_printf("[dbg] ERR SIM pad is disconnected\n");
    else {
        pad_sequence_t sequence; const char *error;
        if (!pad_cmd_parse(argc,argv,&sequence,&error)) debug_printf("[dbg] ERR SIM %s\n",error);
        else if (atomic_load(&outstanding)>=8 || player.count==4) debug_printf("[dbg] ERR SIM queue full\n");
        else {
            uint32_t next_token=debug_async_token();
            atomic_fetch_add(&outstanding,1);
            bool queued=pad_player_enqueue(&player,&sequence,next_token); configASSERT(queued);
            uint32_t budget=0;
            for (unsigned i=0;i<player.count;++i) budget+=player.jobs[(player.head+i)%4].sequence.duration_ms;
            if (sequence.camera_warning) debug_printf("[dbg] SIM WARN camera will receive shutter/record\n");
            debug_printf("[dbg] OK SIM queued token=%lu duration=%lums\n",(unsigned long)next_token,(unsigned long)budget);
        }
    }
    xSemaphoreGive(mutex); return true;
}
#else
esp_err_t pad_console_start(void) { return ESP_OK; }
bool pad_console_command(int argc, char **argv) { (void)argc; (void)argv; return false; }
void pad_console_poll(void) {}
#endif
