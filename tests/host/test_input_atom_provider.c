#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "input_owner.h"
static esp_err_t traced_publish(input_provider_handle_t h,const input_report_t *r);
#define input_provider_publish traced_publish
#include "../../components/app_input_atom/atom_link.c"
#undef input_provider_publish
#include "atom_sim.h"
static int64_t clock_us=1000;
static bool register_ok=true,create_ok=true,bus_ok=true,queued;
static bool removal_failure=true;
static bool custom_command;
static app_message_t inbox,last_reply;
static unsigned stops,deletes,device_returns,received,presses,trace_count;
static uint32_t last_epoch,last_id;
static TaskFunction_t worker;
static atom_sim_t peripheral;
static uint8_t request_bytes[ATOM_REQUEST_SIZE];
static input_owner_t owner;
static const gamepad_caps_t caps={.session=true,.generation=42};
static esp_err_t traced_publish(input_provider_handle_t h,const input_report_t *r)
{
    assert(r->source_epoch && r->report_id && (!r->connected || (r->atom_online && !r->mismatch)));
    if (last_epoch==r->source_epoch) assert(r->report_id>last_id);
    last_epoch=r->source_epoch;last_id=r->report_id;++trace_count;
    esp_err_t err=input_provider_publish(h,r);input_owner_tick(&owner,&caps,input_now_ms());return err;
}
static bool emit(void *context,pad_action_t a)
{ (void)context;if((a.type==PAD_ACTION_S1 || a.type==PAD_ACTION_S2) && a.value)++presses;return true; }
int64_t esp_timer_get_time(void) { return clock_us; }
TickType_t xTaskGetTickCount(void) { return (TickType_t)(clock_us/1000); }
void vTaskDelay(TickType_t ticks) { clock_us+=(int64_t)ticks*1000; }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) { assert(clear==pdTRUE && wait<=25);vTaskDelay(wait);return 0; }
void vTaskDelete(void *unused) { assert(!unused);++deletes; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *context,unsigned priority,void *handle)
{ assert(!strcmp(name,"atom_link") && stack==3072 && priority==4 && !context && !handle);worker=fn;return create_ok?pdPASS:pdFALSE; }
uint32_t app_console_endpoint_generation(app_endpoint_t e) { return e==APP_ENDPOINT_INPUT_ATOM?3:e==APP_ENDPOINT_INPUT?2:e==APP_ENDPOINT_UART?4:0; }
esp_err_t app_console_endpoint_register(app_endpoint_t e,const app_endpoint_config_t *c)
{ assert(e==APP_ENDPOINT_INPUT_ATOM && c->control_depth==4 && c->bulk_depth==1);return register_ok?ESP_OK:ESP_ERR_INVALID_STATE; }
void app_console_endpoint_stop(app_endpoint_t e) { assert(e==APP_ENDPOINT_INPUT_ATOM);++stops; }
esp_err_t app_console_receive(app_endpoint_t e,app_message_t *m,uint32_t timeout)
{
    assert(e==APP_ENDPOINT_INPUT_ATOM && !timeout);if(!queued)return ESP_ERR_TIMEOUT;queued=false;
    if(custom_command){*m=inbox;return ESP_OK;}
    *m=(app_message_t){.type=APP_MESSAGE_INPUT_ATOM_COMMAND,.source=APP_ENDPOINT_INPUT,.target=e,
        .generation=2,.endpoint_epoch=3,.deadline_us=clock_us+100000,.payload.command={.index=APP_INPUT_PROVIDER_PAD_KIND,.value=1}};
    return ESP_OK;
}
esp_err_t app_console_reply(const app_message_t *m,app_message_t *reply) { (void)m;last_reply=*reply;return ESP_OK; }
void app_message_release(app_message_t *m) { assert(!m->lease); }
const char *esp_err_to_name(esp_err_t err) { (void)err;return "fake"; }
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
esp_err_t i2c_master_get_bus_handle(unsigned port,i2c_master_bus_handle_t *bus)
{ assert(port==I2C_NUM_0);*bus=(void *)1;return bus_ok?ESP_OK:ESP_FAIL; }
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,const i2c_device_config_t *config,i2c_master_dev_handle_t *device)
{ assert(bus==(void *)1 && config->dev_addr_length==I2C_ADDR_BIT_LEN_7 && config->device_address==ATOM_PROTOCOL_ADDRESS && config->scl_speed_hz==100000);*device=(void *)2;return ESP_OK; }
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t device)
{ assert(device==(void *)2);if(removal_failure){removal_failure=false;return ESP_FAIL;}++device_returns;return ESP_OK; }
esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus,unsigned address,unsigned timeout)
{ assert(bus==(void *)1 && address==ATOM_PROTOCOL_ADDRESS && timeout==100);return ESP_OK; }
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t device,const uint8_t *bytes,size_t length,unsigned timeout)
{ assert(device==(void *)2 && length==ATOM_REQUEST_SIZE && timeout==100);memcpy(request_bytes,bytes,length);return ESP_OK; }
esp_err_t i2c_master_receive(i2c_master_dev_handle_t device,uint8_t *bytes,size_t length,unsigned timeout)
{
    assert(device==(void *)2 && timeout==100);++received;
    if (received>=4) { if(received==6)atomic_store(&stopping,true);return ESP_ERR_TIMEOUT; }
    if (received==1) assert((atom_read_le(request_bytes+4,4)>>16)==1); /* Restored type in first HELLO. */
    if (received==3) { peripheral.pad.r2=242;atom_sim_pad(&peripheral,&peripheral.pad); }
    size_t produced=0;assert(atom_sim_transact(&peripheral,request_bytes,bytes,&produced)==ATOM_SIM_OK && produced==length);
    if (length==ATOM_POLL_SIZE+7) { bytes[6+27]&=~ATOM_DEBUG_SIM;bytes[length-1]=atom_crc8(bytes,length-1); }
    return ESP_OK;
}
int main(void)
{
    assert(input_provider_registry_init()==ESP_OK);input_owner_init(&owner,emit,NULL);
    bus_ok=false;assert(input_atom_prepare()==ESP_FAIL && !prepared_device && !worker && !stops);
    bus_ok=true;assert(input_atom_prepare()==ESP_OK && input_atom_prepare()==ESP_OK && prepared_device && !worker && !handle && !stops);
    register_ok=false;assert(input_atom_start()==ESP_ERR_INVALID_STATE);
    register_ok=true;create_ok=false;assert(input_atom_start()==ESP_ERR_NO_MEM && stops==1);
    assert(input_atom_stop(0)==ESP_FAIL && !device_returns && prepared_device);
    assert(input_atom_stop(0)==ESP_OK && device_returns==1 && !prepared_device);
    removal_failure=true; /* Worker still retries a failed physical return. */
    input_owner_tick(&owner,&caps,input_now_ms());create_ok=true;bus_ok=false;
    assert(input_atom_start()==ESP_FAIL && !atomic_load(&running) && device_returns==1 && stops==1);
    input_owner_tick(&owner,&caps,input_now_ms());bus_ok=true;atom_sim_init(&peripheral);
    peripheral.pad.connected=true;peripheral.pad.battery=8;atom_sim_pad(&peripheral,&peripheral.pad);
    queued=true;assert(input_atom_start()==ESP_OK && input_atom_start()==ESP_ERR_INVALID_STATE);
    commands();assert(mode_known && observed_mode==1);
    custom_command=true;
    inbox=(app_message_t){.type=APP_MESSAGE_INPUT_ATOM_COMMAND,.source=APP_ENDPOINT_UART,
        .target=APP_ENDPOINT_INPUT_ATOM,.flags=APP_MESSAGE_REQUEST,.generation=4,.endpoint_epoch=3,
        .deadline_us=clock_us+100000,.payload.command={.index=APP_INPUT_ATOM_LOG_MODE,.value=I2C_LOG_ALL}};
    queued=true;commands();assert(last_reply.result==ESP_OK && monitor.mode==I2C_LOG_ALL);
    worker(NULL);
    assert(received==6 && monitor.stats.total==6 && presses==2 && trace_count>=3);
    assert(!owner.latest.connected && !atomic_load(&running) && device_returns==2 && stops==2 && deletes==1 && !prepared_device);
    inbox.deadline_us=clock_us+100000;inbox.payload.command.index=APP_INPUT_ATOM_STATS;
    queued=true;commands();assert(last_reply.result==ESP_OK && last_reply.payload.i2c_stats.total==6 && last_reply.payload.i2c_stats.failed==3);
    inbox.payload.command.index=APP_INPUT_ATOM_LOG_READ;queued=true;commands();
    assert(last_reply.result==ESP_OK && last_reply.payload.i2c.available && last_reply.payload.i2c.frame.request[0]==0xa5);
    inbox.payload.command.index=APP_INPUT_ATOM_STATS;inbox.payload.command.flag=true;queued=true;commands();
    assert(last_reply.result==ESP_OK && !last_reply.payload.i2c_stats.total && monitor.mode==I2C_LOG_ALL);
    assert(input_atom_stop(0)==ESP_OK);
    input_owner_tick(&owner,&caps,input_now_ms());assert(input_atom_start()==ESP_OK);
    assert(input_atom_stop(0)==ESP_ERR_TIMEOUT);worker(NULL);assert(!atomic_load(&running) && device_returns==3);
    puts("Physical ATOM protocol, first HELLO type, report ownership, failures and device cleanup passed");
}
