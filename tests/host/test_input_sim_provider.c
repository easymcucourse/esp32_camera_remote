#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_input_sim/input_sim.c"
#include "input_owner.h"
static int64_t clock_us=1000;
static bool register_ok=true,create_ok=true,send_ok=true;
static unsigned returned_count,deleted,stops,presses,records;
static uint32_t uart_generation=4;
static TaskFunction_t worker;
static app_message_t last_result;
int64_t esp_timer_get_time(void) { return clock_us; }
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)(clock_us/1000); }
void vTaskDelay(TickType_t ticks) { clock_us+=(int64_t)ticks*1000; }
void vTaskDelayUntil(TickType_t *previous,TickType_t ticks)
{ *previous+=ticks;clock_us=(int64_t)*previous*1000;atomic_store(&stopping,true); }
void vTaskDelete(void *unused) { assert(!unused);++deleted; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{ assert(!strcmp(name,"lcd_pad_player") && stack==3072 && priority==3 && !context && !handle);worker=fn;return create_ok?pdPASS:pdFALSE; }
uint32_t app_console_endpoint_generation(app_endpoint_t e)
{ return e==APP_ENDPOINT_INPUT_SIM?3:e==APP_ENDPOINT_INPUT?2:e==APP_ENDPOINT_UART?uart_generation:e==APP_ENDPOINT_UI?5:0; }
esp_err_t app_console_endpoint_register(app_endpoint_t e,const app_endpoint_config_t *c)
{ assert(e==APP_ENDPOINT_INPUT_SIM && c->control_depth==8 && c->bulk_depth==4);return register_ok?ESP_OK:ESP_ERR_INVALID_STATE; }
void app_console_endpoint_stop(app_endpoint_t e) { assert(e==APP_ENDPOINT_INPUT_SIM);++stops; }
esp_err_t app_console_send(app_message_t *m)
{ assert(m->source==APP_ENDPOINT_INPUT_SIM && m->target==APP_ENDPOINT_UART && m->flags==APP_MESSAGE_EVENT && !m->lease);last_result=*m;return send_ok?ESP_OK:ESP_ERR_NO_MEM; }
esp_err_t app_console_receive(app_endpoint_t e,app_message_t *m,uint32_t timeout)
{ assert(e==APP_ENDPOINT_INPUT_SIM && !timeout);(void)m;return ESP_ERR_TIMEOUT; }
esp_err_t app_console_reply(const app_message_t *m,app_message_t *reply) { (void)m;(void)reply;return ESP_OK; }
static void returned(void *context) { assert(context);++returned_count; }
static bool emit(void *context,pad_action_t a)
{ (void)context;if ((a.type==PAD_ACTION_S1 || a.type==PAD_ACTION_S2) && a.value) ++presses;if(a.type==PAD_ACTION_RECORD)++records;return true; }
static app_message_t scalar(unsigned op,uint32_t value)
{
    return (app_message_t){.type=APP_MESSAGE_INPUT_SIM_COMMAND,.source=APP_ENDPOINT_UART,
        .target=APP_ENDPOINT_INPUT_SIM,.flags=APP_MESSAGE_REQUEST,.generation=4,.endpoint_epoch=3,
        .deadline_us=clock_us+100000,.payload.command={.index=op,.value=value}};
}
static void poll(input_owner_t *owner,const gamepad_caps_t *caps)
{ clock_us+=50000;poll_report();input_owner_tick(owner,caps,now_ms()); }
static esp_err_t enqueue(pad_sequence_t *seq,uint32_t token)
{
    app_message_t m=scalar(APP_INPUT_SIM_SEQUENCE,0),reply={0};
    m.flags|=APP_MESSAGE_BULK;m.payload.command.token=token;
    assert(app_message_lease_create(seq,sizeof(*seq),false,returned,seq,&m.lease)==ESP_OK);
    esp_err_t err=command(&m,&reply);app_message_release(&m);return err;
}
int main(void)
{
    assert(input_provider_registry_init()==ESP_OK);
    input_owner_t owner;gamepad_caps_t caps={.session=true,.generation=42,.recording_known=true};
    input_owner_init(&owner,emit,NULL);assert(input_owner_select(&owner,INPUT_SOURCE_UART_SIM));
    register_ok=false;assert(input_sim_start()==ESP_ERR_INVALID_STATE && !atomic_load(&running));
    register_ok=true;create_ok=false;assert(input_sim_start()==ESP_ERR_NO_MEM && stops==1);
    input_owner_tick(&owner,&caps,now_ms());create_ok=true;
    assert(input_sim_start()==ESP_OK && worker && input_sim_start()==ESP_ERR_INVALID_STATE);
    app_message_t m=scalar(APP_INPUT_SIM_PAD_KIND,1),reply={0};
    assert(command(&m,&reply)==ESP_ERR_INVALID_ARG);
    m.source=APP_ENDPOINT_INPUT;m.generation=2;m.flags=0;
    assert(command(&m,&reply)==ESP_OK && mode_known && mode==1);
    m=scalar(APP_INPUT_SIM_ENABLE,1);m.deadline_us=clock_us;
    assert(command(&m,&reply)==ESP_ERR_TIMEOUT && !enabled);
    m.deadline_us=clock_us+100000;m.endpoint_epoch=2;
    assert(command(&m,&reply)==ESP_ERR_INVALID_STATE && !enabled);
    m.endpoint_epoch=3;assert(command(&m,&reply)==ESP_OK && enabled);
    m=scalar(APP_INPUT_SIM_STATUS,0);m.source=APP_ENDPOINT_UI;m.generation=5;
    assert(command(&m,&reply)==ESP_OK && reply.payload.command.flag);
    m.payload.command.index=APP_INPUT_SIM_ENABLE;m.payload.command.value=0;
    assert(command(&m,&reply)==ESP_ERR_INVALID_ARG && enabled);
    m=scalar(APP_INPUT_SIM_CONNECT,1);assert(command(&m,&reply)==ESP_OK);
    m=scalar(APP_INPUT_SIM_BATTERY,7);assert(command(&m,&reply)==ESP_OK);
    m=scalar(APP_INPUT_SIM_GIMBAL,3);assert(command(&m,&reply)==ESP_OK);
    poll(&owner,&caps);poll(&owner,&caps);poll(&owner,&caps);
    assert(owner.latest.connected && owner.latest.sim && !owner.latest.atom_online &&
        owner.latest.kind==1 && owner.latest.battery==7 && owner.latest.gimbal==3 && presses==0);
    m=scalar(APP_INPUT_SIM_CRC,2);assert(command(&m,&reply)==ESP_OK);
    poll(&owner,&caps);poll(&owner,&caps);assert(owner.latest.connected && client.failures==2);
    poll(&owner,&caps);assert(owner.latest.connected && !client.failures);
    m=scalar(APP_INPUT_SIM_FAIL,3);assert(command(&m,&reply)==ESP_OK);
    poll(&owner,&caps);poll(&owner,&caps);assert(owner.latest.connected);
    poll(&owner,&caps);assert(!owner.latest.connected && !client.online);
    clock_us+=1000000;poll(&owner,&caps);poll(&owner,&caps);poll(&owner,&caps);
    assert(owner.latest.connected && !client.failures && !presses);
    pad_sequence_t seq={.count=3,.duration_ms=100,.actions={
        {.kind=PAD_TRIGGER,.side=1,.value=242},{.kind=PAD_WAIT,.value=100},{.kind=PAD_TRIGGER,.side=1,.value=0}}};
    m=scalar(APP_INPUT_SIM_SEQUENCE,0);m.flags|=APP_MESSAGE_BULK;m.payload.command.token=10;
    assert(app_message_lease_create(&seq,sizeof(seq),false,returned,&seq,&m.lease)==ESP_OK);
    assert(command(&m,&reply)==ESP_OK && reply.payload.command.token==10 && reply.payload.command.duration_ms==100);
    assert(command(&m,&reply)==ESP_ERR_INVALID_STATE); /* Token collision. */
    seq.actions[0].value=0;app_message_release(&m);assert(returned_count==1);
    pad_player_tick(&player,now_ms(),&ops);poll(&owner,&caps);assert(presses==2); /* Copied before lease return. */
    pad_player_tick(&player,now_ms(),&ops);poll(&owner,&caps);
    pad_player_tick(&player,now_ms(),&ops);poll(&owner,&caps);
    assert(!player.count && owner.latest.rt==0 && presses==2);
    send_ok=false;flush_results();bool pending=false;
    for (unsigned i=0;i<8;++i) pending|=completed[i].used;
    assert(pending && last_result.payload.command.token==10 && !last_result.payload.command.flag);
    send_ok=true;flush_results();for(unsigned i=0;i<8;++i)assert(!completed[i].used);
    seq.actions[0].side=0;seq.actions[0].value=242;seq.actions[2].side=0;
    m=scalar(APP_INPUT_SIM_SEQUENCE,0);m.flags|=APP_MESSAGE_BULK;m.payload.command.token=11;
    assert(app_message_lease_create(&seq,sizeof(seq),true,returned,&seq,&m.lease)==ESP_OK);
    assert(command(&m,&reply)==ESP_ERR_INVALID_ARG);app_message_release(&m);
    assert(app_message_lease_create(&seq,sizeof(seq),false,returned,&seq,&m.lease)==ESP_OK);
    assert(command(&m,&reply)==ESP_OK);app_message_release(&m);
    pad_player_tick(&player,now_ms(),&ops);poll(&owner,&caps);assert(records==1);
    m=scalar(APP_INPUT_SIM_ONLINE,0);assert(command(&m,&reply)==ESP_OK);
    input_owner_tick(&owner,&caps,now_ms());assert(!owner.latest.connected && !player.count);
    flush_results();assert(last_result.payload.command.token==11 && last_result.payload.command.flag);
    m=scalar(APP_INPUT_SIM_VERSION,256);assert(command(&m,&reply)==ESP_ERR_INVALID_ARG);
    m=scalar(APP_INPUT_SIM_ONLINE,1);assert(command(&m,&reply)==ESP_OK);
    m=scalar(APP_INPUT_SIM_VERSION,3);assert(command(&m,&reply)==ESP_OK);
    poll(&owner,&caps);assert(owner.latest.mismatch && !owner.latest.connected);
    m=scalar(APP_INPUT_SIM_VERSION,2);assert(command(&m,&reply)==ESP_OK);
    poll(&owner,&caps);poll(&owner,&caps);assert(!owner.latest.mismatch);
    m=scalar(APP_INPUT_SIM_CONNECT,1);assert(command(&m,&reply)==ESP_OK);send_ok=false;
    for(unsigned i=0;i<4;++i)assert(enqueue(&seq,20+i)==ESP_OK);
    assert(enqueue(&seq,24)==ESP_ERR_NO_MEM); /* Four player jobs. */
    for(unsigned i=0;i<5;++i){clock_us+=1000000;pad_player_tick(&player,now_ms(),&ops);}
    flush_results();assert(!player.count);
    for(unsigned i=0;i<4;++i)assert(enqueue(&seq,24+i)==ESP_OK);
    assert(enqueue(&seq,28)==ESP_ERR_NO_MEM); /* Eight reserved results, even with UART full. */
    send_ok=true;
    assert(input_sim_stop(0)==ESP_ERR_TIMEOUT && atomic_load(&running));worker(NULL);
    assert(!atomic_load(&running) && deleted==1 && stops==2);
    input_owner_tick(&owner,&caps,now_ms());assert(!owner.latest.connected);
    assert(input_sim_start()==ESP_OK);worker(NULL);assert(!atomic_load(&running));
    /* Loss of UART also cancels a previously completed HOLD with no pending
     * completion record, rather than leaving a raw trigger pressed forever. */
    input_owner_tick(&owner,&caps,now_ms());assert(input_sim_start()==ESP_OK && provider);
    enabled=true;controller_generation=4;player.state=(pad_state_t){.connected=true,.r2=242};
    uart_generation=5;retire_controller();assert(!enabled && !player.state.connected && !player.state.r2);
    assert(input_sim_stop(0)==ESP_ERR_TIMEOUT);worker(NULL);assert(!atomic_load(&running));
    assert(input_sim_stop(0)==ESP_OK);
    puts("Independent SIM protocol reports, leases, player results, barriers and lifecycle passed");
}
