#include "input_sim.h"
#include "input_provider.h"
#include "app_console.h"
#include "atom_sim.h"
#include "atom_client.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

static atomic_bool running,stopping;
static input_provider_handle_t provider;
static atom_sim_t sim;
static atom_client_t client;
static pad_player_t player;
static bool enabled,mode_known,discard_cached;
static uint8_t source_tag;
static unsigned mode;
static uint32_t epoch,id,next_poll,controller_generation;
typedef struct { uint32_t token,elapsed,source_generation; bool used,done,cancelled; } completion_t;
static completion_t completed[8];
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)esp_timer_get_time()/1000); }
static void apply(void *unused,const pad_state_t *state) { (void)unused;atom_sim_pad(&sim,state); }
static void done(void *unused,uint32_t token,uint32_t elapsed,bool cancelled)
{
    (void)unused;
    for (unsigned i=0;i<8;++i) if (completed[i].used && completed[i].token==token) {
        completed[i].done=true;completed[i].cancelled=cancelled;completed[i].elapsed=elapsed;return;
    }
}
static const pad_player_ops_t ops={apply,done,NULL};
static void restart_source(void)
{
    input_provider_disconnect(provider,INPUT_DISCONNECT_RESTART);
    if (epoch==UINT32_MAX) { atomic_store(&stopping,true);return; }
    ++epoch;id=0;discard_cached=true;source_tag=0;next_poll=now_ms();
}
static void cancel_player(void) { pad_player_cancel(&player,now_ms(),&ops); }
static void retire_controller(void)
{
    /* A completed HOLD may leave buttons/triggers held after its result was
     * delivered. Closing/restarting UART must cancel that raw state as well
     * as outstanding jobs; completion records alone cannot detect it. */
    if (enabled && controller_generation!=app_console_endpoint_generation(APP_ENDPOINT_UART)) {
        enabled=false;cancel_player();restart_source();client=(atom_client_t){.input_mode=mode};
    }
}
static bool sequence_valid(const pad_sequence_t *s)
{
    if (!s || !s->count || s->count>PAD_ACT_MAX) return false;
    uint32_t duration=0;
    for (unsigned i=0;i<s->count;++i) {
        const pad_act_t *a=&s->actions[i];
        if (a->kind<PAD_PRESS || a->kind>PAD_WAIT || a->side>1 || a->mask>0x3ffffu) return false;
        if (a->kind==PAD_STICK && (a->x<-128 || a->x>127 || a->y<-128 || a->y>127)) return false;
        if (a->kind==PAD_TRIGGER && a->value>255) return false;
        if (a->kind==PAD_WAIT) { if (!a->value || a->value>10000) return false;duration+=a->value; }
    }
    return s->duration_ms==duration;
}
static esp_err_t command(const app_message_t *m,app_message_t *reply)
{
    unsigned op=m->payload.command.index;uint32_t value=m->payload.command.value;
    if (m->type!=APP_MESSAGE_INPUT_SIM_COMMAND || m->target!=APP_ENDPOINT_INPUT_SIM ||
        !m->generation || m->generation!=app_console_endpoint_generation(m->source) ||
        m->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_INPUT_SIM)) return ESP_ERR_INVALID_STATE;
    if (m->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (atomic_load(&stopping)) return ESP_ERR_INVALID_STATE;
    if (op==APP_INPUT_SIM_PAD_KIND) {
        if (m->source!=APP_ENDPOINT_INPUT || m->flags || m->lease || value>1) return ESP_ERR_INVALID_ARG;
        if (!mode_known || mode!=value) {
            mode=value;mode_known=true;restart_source();client=(atom_client_t){.input_mode=mode};
        }
        return ESP_OK;
    }
    if ((m->source!=APP_ENDPOINT_UART && !(m->source==APP_ENDPOINT_UI && op==APP_INPUT_SIM_STATUS)) ||
        m->flags!=(APP_MESSAGE_REQUEST|(op==APP_INPUT_SIM_SEQUENCE?APP_MESSAGE_BULK:0)) ||
        (op!=APP_INPUT_SIM_SEQUENCE && m->lease)) return ESP_ERR_INVALID_ARG;
    if (op==APP_INPUT_SIM_STATUS) {
        reply->payload.command.flag=enabled;reply->payload.command.value=player.count;return ESP_OK;
    }
    if (op==APP_INPUT_SIM_ENABLE) {
        if (value>1) return ESP_ERR_INVALID_ARG;
        cancel_player();atom_sim_init(&sim);enabled=value!=0;
        controller_generation=m->generation;
        restart_source();client=(atom_client_t){.input_mode=mode};return ESP_OK;
    }
    if (!enabled) return ESP_ERR_INVALID_STATE;
    switch(op) {
    case APP_INPUT_SIM_ONLINE:
        if (value>1) return ESP_ERR_INVALID_ARG;
        if (!value) { cancel_player();restart_source();client=(atom_client_t){.input_mode=mode}; }
        sim.online=value!=0;next_poll=now_ms();return ESP_OK;
    case APP_INPUT_SIM_REBOOT:
        cancel_player();atom_sim_reboot(&sim);restart_source();client=(atom_client_t){.input_mode=mode};
        ESP_LOGW("input_sim","SIM protocol restarted boot_id=%lu",(unsigned long)sim.boot_id);
        reply->payload.command.value=sim.boot_id;return ESP_OK;
    case APP_INPUT_SIM_VERSION:
        if (value>255) return ESP_ERR_INVALID_ARG;
        sim.version=value;next_poll=now_ms();return ESP_OK;
    case APP_INPUT_SIM_FAIL: case APP_INPUT_SIM_CRC: case APP_INPUT_SIM_TIMEOUT:
        if (value>10000) return ESP_ERR_INVALID_ARG;
        if (op==APP_INPUT_SIM_FAIL) sim.fail=value;
        else if (op==APP_INPUT_SIM_CRC) sim.crc=value;else sim.timeout=value;
        return ESP_OK;
    case APP_INPUT_SIM_GIMBAL:
        if (value>3) return ESP_ERR_INVALID_ARG;
        sim.gimbal=value;return ESP_OK;
    case APP_INPUT_SIM_CONNECT:
        if (value>1) return ESP_ERR_INVALID_ARG;
        cancel_player();player.state.connected=value!=0;apply(NULL,&player.state);return ESP_OK;
    case APP_INPUT_SIM_BATTERY:
        if (value>10 && value!=255) return ESP_ERR_INVALID_ARG;
        player.state.battery=value;apply(NULL,&player.state);return ESP_OK;
    case APP_INPUT_SIM_GAP: case APP_INPUT_SIM_OVERFLOW:
        atom_sim_gap(&sim,op==APP_INPUT_SIM_OVERFLOW);return ESP_OK;
    case APP_INPUT_SIM_SEQUENCE: {
        if (!player.state.connected || !sim.online || !m->payload.command.token) return ESP_ERR_INVALID_STATE;
        size_t length=0;
        const pad_sequence_t *s=app_message_lease_data(m->lease,&length);
        if (length!=sizeof(*s) || !sequence_valid(s) || app_message_lease_write(m->lease,NULL)) return ESP_ERR_INVALID_ARG;
        completion_t *slot=NULL;
        for (unsigned i=0;i<8;++i) {
            if (completed[i].used && completed[i].token==m->payload.command.token) return ESP_ERR_INVALID_STATE;
            if (!completed[i].used) slot=&completed[i];
        }
        if (!slot || player.count==4) return ESP_ERR_NO_MEM;
        if (!pad_player_enqueue(&player,s,m->payload.command.token)) return ESP_ERR_INVALID_ARG;
        *slot=(completion_t){.used=true,.token=m->payload.command.token,.source_generation=m->generation};
        uint32_t budget=0;
        for (unsigned i=0;i<player.count;++i) budget+=player.jobs[(player.head+i)%4].sequence.duration_ms;
        reply->payload.command.token=m->payload.command.token;reply->payload.command.duration_ms=budget;
        return ESP_OK;
    }
    default:return ESP_ERR_NOT_SUPPORTED;
    }
}
static void flush_results(void)
{
    for (unsigned i=0;i<8;++i) {
        completion_t *r=&completed[i];if (!r->used || !r->done) continue;
        if (r->source_generation!=app_console_endpoint_generation(APP_ENDPOINT_UART)) { *r=(completion_t){0};continue; }
        app_message_t m={.type=APP_MESSAGE_INPUT_SIM_COMMAND,.source=APP_ENDPOINT_INPUT_SIM,
            .target=APP_ENDPOINT_UART,.flags=APP_MESSAGE_EVENT,
            .generation=app_console_endpoint_generation(APP_ENDPOINT_INPUT_SIM)};
        m.payload.command.index=APP_INPUT_SIM_SEQUENCE;m.payload.command.token=r->token;
        m.payload.command.duration_ms=r->elapsed;m.payload.command.flag=r->cancelled;
        if (app_console_send(&m)==ESP_OK) *r=(completion_t){0};
    }
}
static void poll_report(void)
{
    uint32_t now=now_ms();if ((int32_t)(now-next_poll)<0) return;
    next_poll=now+50;
    if (!enabled || !mode_known) return;
    input_report_t report={.sim=true,.kind=mode,.battery=255};
    if (sim.online) {
        atom_request_t request=atom_client_request(&client);
        uint8_t raw[ATOM_REQUEST_SIZE],bytes[ATOM_RESPONSE_MAX];size_t size;
        atom_encode_request(raw,request);
        atom_sim_result_t io=atom_sim_transact(&sim,raw,bytes,&size);
        const uint8_t *p=NULL;
        atom_client_result_t result=io==ATOM_SIM_OK?atom_client_response(&client,bytes,size,&p):atom_client_failure(&client);
        if (result==ATOM_CLIENT_RESTART || result==ATOM_CLIENT_HELLO || result==ATOM_CLIENT_OFFLINE || result==ATOM_CLIENT_MISMATCH) {
            restart_source();
            if (result==ATOM_CLIENT_OFFLINE || result==ATOM_CLIENT_MISMATCH) next_poll=now+(client.mismatch?5000:1000);
            if (result==ATOM_CLIENT_OFFLINE) ESP_LOGW("input_sim","SIM link lost; old input released");
            if (result==ATOM_CLIENT_MISMATCH) ESP_LOGW("input_sim","SIM firmware version mismatch / HELLO failed");
        } else if (result==ATOM_CLIENT_POLL) {
            if (source_tag!=p[27]) { restart_source();source_tag=p[27]; }
            report.connected=p[4]==3;report.buttons=report.connected?atom_read_le(p+11,3):0;
            report.rx=(int8_t)p[14];report.ry=(int8_t)p[15];report.lt=p[16];report.rt=p[17];
            report.battery=p[8];report.gimbal=p[5];report.gap=discard_cached || (p[18]&2)!=0;
            bool valid=(p[18]&1)!=0;
            if (valid) {
                uint32_t ack=atom_read_le(p+19,4);
                if (ack!=client.ack_id) {
                    report.event_valid=report.connected && !discard_cached;
                    report.event_buttons=atom_read_le(p+23,3);client.ack_id=ack;
                }
            }
            if (report.connected && discard_cached && !valid) discard_cached=false;
        }
        /* RETRY must not publish a false disconnect on the first bad frame.
         * Existing three-failure protocol threshold remains authoritative. */
        if (result==ATOM_CLIENT_RETRY) return;
    } else next_poll=now+1000;
    report.mismatch=client.mismatch;
    if (id==UINT32_MAX) restart_source();
    if (atomic_load(&stopping)) return;
    report.source_epoch=epoch;report.report_id=++id;
    input_provider_publish(provider,&report);
}
static void task(void *unused)
{
    (void)unused;TickType_t cycle=xTaskGetTickCount();
    while (!atomic_load(&stopping)) {
        retire_controller();
        app_message_t m;
        for (unsigned i=0;i<8 && app_console_receive(APP_ENDPOINT_INPUT_SIM,&m,0)==ESP_OK;++i) {
            app_message_t reply={0};reply.result=command(&m,&reply);
            if (m.flags&APP_MESSAGE_REQUEST) app_console_reply(&m,&reply);
            app_message_release(&m);
        }
        retire_controller();pad_player_tick(&player,now_ms(),&ops);poll_report();flush_results();
        vTaskDelayUntil(&cycle,pdMS_TO_TICKS(10));
    }
    cancel_player();input_provider_disconnect(provider,INPUT_DISCONNECT_STOP);
    input_provider_unregister(provider);provider=0;flush_results();
    /* Reserved completion records survive a full UART queue. A stop timeout
     * keeps this sole owner alive; it never deletes a worker still returning
     * accepted sequence cancellations. */
    for (;;) {
        bool pending=false;
        for (unsigned i=0;i<8;++i) pending|=completed[i].used;
        if (!pending) break;
        flush_results();vTaskDelay(pdMS_TO_TICKS(10));
    }
    app_console_endpoint_stop(APP_ENDPOINT_INPUT_SIM);atomic_store(&running,false);vTaskDelete(NULL);
}
esp_err_t input_sim_start(void)
{
    if (atomic_load(&running)) return ESP_ERR_INVALID_STATE;
    const app_endpoint_config_t config={8,4};
    esp_err_t err=app_console_endpoint_register(APP_ENDPOINT_INPUT_SIM,&config);if (err!=ESP_OK) return err;
    err=input_provider_register(INPUT_SOURCE_UART_SIM,&provider);
    if (err!=ESP_OK) { app_console_endpoint_stop(APP_ENDPOINT_INPUT_SIM);return err; }
    atom_sim_init(&sim);client=(atom_client_t){0};player=(pad_player_t){.state.battery=255};
    for (unsigned i=0;i<8;++i) completed[i]=(completion_t){0};
    epoch=1;id=0;controller_generation=0;source_tag=0;
    enabled=mode_known=false;discard_cached=true;next_poll=now_ms();
    atomic_store(&stopping,false);atomic_store(&running,true);
    if (xTaskCreate(task,"lcd_pad_player",3072,NULL,3,NULL)!=pdPASS) {
        input_provider_unregister(provider);provider=0;app_console_endpoint_stop(APP_ENDPOINT_INPUT_SIM);
        atomic_store(&running,false);return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t input_sim_stop(uint32_t timeout_ms)
{
    atomic_store(&stopping,true);int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&running)) {
        if (esp_timer_get_time()>=deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}
