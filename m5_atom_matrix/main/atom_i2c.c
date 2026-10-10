#include "atom_i2c.h"
#include "gimbal_link.h"
#include "atom_protocol.h"
#include "i2c_debug.h"
#include "atom_fault.h"
#include "debug_console.h"
#include "debug_hex.h"
#include "atom_slave_tx.h"
#include "matrix_status.h"
#include "ds4_host.h"
#include "driver/i2c_slave.h"
#include "esp_check.h"
#include "esp_attr.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "atom_i2c";
static atomic_bool ready, button, receive_gap, heartbeat_seen;
static atomic_uint presses, heartbeat_ms, faults, invalid_requests;
static uint32_t boot_id, last_dropped;
static i2c_slave_dev_handle_t slave;
static QueueHandle_t received_chunks;
#if CONFIG_REMOTE_DBG_SIM
static atom_fault_t injection;
typedef struct { uint32_t token; uint8_t bytes[ATOM_REQUEST_SIZE]; } debug_request_t;
typedef struct { uint32_t token; uint8_t bytes[ATOM_RESPONSE_MAX],length; } debug_reply_t;
static QueueHandle_t debug_requests,debug_replies;
static atomic_bool debug_pending;
#endif
typedef struct { uint32_t timestamp; uint16_t length; uint8_t bytes[128]; } receive_chunk_t;
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS); }
void atom_i2c_ready(void) { atomic_store(&ready, true); }
void atom_i2c_button(bool pressed, uint16_t count)
{ atomic_store(&button, pressed); atomic_store(&presses, count); }
bool atom_i2c_online(void)
{ return atomic_load(&heartbeat_seen) && (uint32_t)(now_ms() - atomic_load(&heartbeat_ms)) <= 1500; }
uint8_t atom_i2c_faults(void)
{ return (uint8_t)atomic_load(&faults) | (matrix_status_faults() & (MATRIX_PROTOCOL | MATRIX_BLUETOOTH)) |
    (gimbal_link_fault()?ATOM_FAULT_GIMBAL:0); }

void atom_i2c_get_status(uint32_t *age_ms, uint32_t *invalid)
{
    *age_ms = atomic_load(&heartbeat_seen) ? now_ms() - atomic_load(&heartbeat_ms) : UINT32_MAX;
    *invalid = atomic_load(&invalid_requests);
}
static bool IRAM_ATTR receive_callback(i2c_slave_dev_handle_t handle,
                              const i2c_slave_rx_done_event_data_t *event, void *context)
{
    (void)handle; (void)context;
    BaseType_t woken = pdFALSE;
    if (!event->length) return false;
    if (event->length > 128) { atomic_store(&receive_gap, true); return false; }
    receive_chunk_t chunk;
    chunk.timestamp = (uint32_t)((uint64_t)xTaskGetTickCountFromISR() * portTICK_PERIOD_MS);
    chunk.length = (uint16_t)event->length;
    memcpy(chunk.bytes, event->buffer, chunk.length);
    if (xQueueSendFromISR(received_chunks, &chunk, &woken) != pdTRUE) atomic_store(&receive_gap, true);
    return woken == pdTRUE;
}
static void respond(atom_request_t request, atom_status_t status,const uint8_t *raw,uint32_t received_at,uint32_t debug_token)
{
        uint8_t payload[ATOM_POLL_SIZE] = {0}, length = 0;
        if (status == ATOM_OK) {
            if (request.cmd == ATOM_CMD_HELLO) {
                if ((request.param & 0xfffe0000) || (request.param & 0xff) > 2 ||
                    ((request.param >> 8) & 0xff) < 2) status = ATOM_BAD_VERSION;
                else {
                    atom_write_le(payload, boot_id, 4);
                    payload[4] = 2; payload[5] = 0;
                    payload[6] = 1 | ATOM_FEATURE_GIMBAL | 8 | ATOM_FEATURE_INPUT_MODE;
                    ds4_host_set_input_mode((request.param >> 16) & 1);
                    payload[7] = DS4_EVENT_CAPACITY;
                    atom_write_le(payload + 8, ATOM_LOCAL_MASK, 3);
                    length = ATOM_HELLO_SIZE;
                }
            } else if (request.cmd == ATOM_CMD_POLL) {
                if (!atomic_load(&ready)) status = ATOM_NOT_READY;
                else {
                    ds4_state_t pad; ds4_event_t event; uint8_t link, remaining; uint32_t dropped;
                    uint8_t source_tag;
                    bool valid = ds4_host_poll(request.param, &pad, &link, &event, &remaining, &dropped, &source_tag);
                    bool simulated=(source_tag&ATOM_DEBUG_SIM)!=0;
                    atom_write_le(payload, boot_id, 4);
                    payload[4] = link;
                    payload[5] = gimbal_link_state();
                    payload[6] = atomic_load(&button);
                    if (valid && event.gap) atomic_fetch_or(&faults, 1);
                    else atomic_fetch_and(&faults, ~1u);
                    payload[7] = atom_i2c_faults();
                    payload[8] = pad.connected ? pad.battery : 255;
                    payload[27] = source_tag;
                    atom_write_le(payload + 9, atomic_load(&presses), 2);
                    if (pad.connected) {
                        atom_write_le(payload + 11, pad.buttons & ATOM_BUTTON_MASK & ~ATOM_LOCAL_MASK, 3);
                        payload[14] = (uint8_t)pad.rx; payload[15] = (uint8_t)pad.ry;
                        payload[16] = pad.l2; payload[17] = pad.r2;
                    }
                    if (valid) {
                        payload[18] = 1 | (event.gap ? 2 : 0);
                        atom_write_le(payload + 19, event.id, 4);
                        atom_write_le(payload + 23, event.buttons, 3);
                        payload[26] = remaining;
                    }
                    if (dropped != last_dropped) {
                        ESP_LOGW(TAG, "%sEvent overflow: dropped=%lu",simulated?"SIM ":"", (unsigned long)dropped);
                        last_dropped = dropped;
                    }
                    length = ATOM_POLL_SIZE;
                }
            } else status = ATOM_UNKNOWN_CMD;
        }
        bool valid = status == ATOM_OK || status == ATOM_NOT_READY;
        if (!debug_token) {
            if (!valid) atomic_fetch_add(&invalid_requests, 1);
            matrix_status_note_lcd_command(valid);
            if (valid) { atomic_store(&heartbeat_ms, now_ms()); atomic_store(&heartbeat_seen, true); }
        }
        if (status == ATOM_OK && request.cmd == ATOM_CMD_POLL) payload[7] = atom_i2c_faults();
        uint8_t reply[ATOM_RESPONSE_MAX];
        size_t size = atom_encode_response(reply, request, status, payload, length);
#if CONFIG_REMOTE_DBG_SIM
        if (debug_token) {
            debug_reply_t result={.token=debug_token,.length=size}; memcpy(result.bytes,reply,size);
            BaseType_t sent=xQueueSend(debug_replies,&result,0); configASSERT(sent==pdTRUE); return;
        }
#endif
        atom_fault_plan_t plan={0};
#if CONFIG_REMOTE_DBG_SIM
        if (valid) plan=atom_fault_take(&injection);
#endif
        esp_err_t err=ESP_OK;
        if (plan.drop || plan.delay_ms) err=atom_slave_replace_reply(slave,NULL,0);
        if (plan.delay_ms) vTaskDelay(pdMS_TO_TICKS(plan.delay_ms));
        if (plan.corrupt) reply[size-1]^=0xff;
        if (!plan.drop && err==ESP_OK) err=atom_slave_replace_reply(slave,reply,size);
        i2c_frame_record_t record={.timestamp=received_at,.elapsed_ms=now_ms()-received_at,.response_len=size};
        memcpy(record.request,raw,ATOM_REQUEST_SIZE); memcpy(record.response,reply,size);
        if (plan.drop) record.response_len=0;
        record.result=err!=ESP_OK?I2C_MON_IO:plan.drop?I2C_MON_TIMEOUT:plan.corrupt?I2C_MON_BAD_CRC:status==ATOM_OK?I2C_MON_OK:status==ATOM_BAD_CRC?I2C_MON_BAD_CRC:
            status==ATOM_BAD_VERSION || status==ATOM_UNKNOWN_CMD?I2C_MON_BAD_HEADER:
            status==ATOM_BAD_PARAM?I2C_MON_BAD_PARAM:I2C_MON_REMOTE;
        i2c_debug_record(&record);
}
bool atom_i2c_debug_command(int argc,char **argv)
{
#if CONFIG_REMOTE_DBG_SIM
    if (strcmp(argv[0],"i2c")) return false;
    if (argc>=2 && !strcmp(argv[1],"req")) {
        debug_request_t request={0}; bool expected=false;
        if (!debug_hex_bytes(argc-2,argv+2,request.bytes,sizeof(request.bytes))) {
            debug_printf("[dbg] ERR SIM i2c req expects exactly 9 hex bytes\n"); return true;
        }
        if (!atomic_compare_exchange_strong(&debug_pending,&expected,true)) {
            debug_printf("[dbg] ERR SIM i2c req busy\n"); return true;
        }
        request.token=debug_async_token();
        if (xQueueSend(debug_requests,&request,0)!=pdTRUE) {
            atomic_store(&debug_pending,false); debug_printf("[dbg] ERR SIM i2c req queue full\n"); return true;
        }
        if (atom_i2c_online()) debug_printf("[dbg] SIM WARN lcd online, request ACK affects live events\n");
        debug_printf("[dbg] OK SIM i2c req queued token=%lu\n",(unsigned long)request.token); return true;
    }
    if (argc==3 && (!strcmp(argv[1],"drop") || !strcmp(argv[1],"corrupt") || !strcmp(argv[1],"delay"))) {
        char *end; unsigned long n=strtoul(argv[2],&end,10);
        unsigned max=!strcmp(argv[1],"delay")?200:10000;
        if (!*argv[2] || *end || argv[2][0]=='-' || n>max) {
            debug_printf("[dbg] ERR SIM i2c %s range 0..%u\n",argv[1],max); return true;
        }
        atomic_uint *field=!strcmp(argv[1],"drop")?&injection.drop:!strcmp(argv[1],"corrupt")?&injection.corrupt:&injection.delay_ms;
        atomic_store(field,(unsigned)n);
        debug_printf("[dbg] OK SIM i2c %s=%lu RAM only\n",argv[1],n); return true;
    }
#else
    (void)argc; (void)argv;
#endif
    return false;
}
void atom_i2c_debug_poll(void)
{
#if CONFIG_REMOTE_DBG_SIM
    debug_reply_t reply;
    if (xQueueReceive(debug_replies,&reply,0)!=pdTRUE) return;
    static const char *const names[]={"OK","BAD_CRC","BAD_VERSION","UNKNOWN_CMD","BAD_PARAM","NOT_READY"};
    char hex[ATOM_RESPONSE_MAX*2+1];
    for (unsigned i=0;i<reply.length;++i) {
        static const char digits[]="0123456789abcdef";
        hex[i*2]=digits[reply.bytes[i]>>4]; hex[i*2+1]=digits[reply.bytes[i]&15];
    }
    hex[reply.length*2]=0;
    debug_printf("[dbg] DONE SIM i2c req token=%lu status=%s reply=%s\n",(unsigned long)reply.token,
        reply.length?names[reply.bytes[4]]:"NO_RESPONSE",reply.length?hex:"-");
    atomic_store(&debug_pending,false);
#endif
}
static void i2c_task(void *context)
{
    (void)context;
    atom_receiver_t receiver = {0};
    for (;;) {
#if CONFIG_REMOTE_DBG_SIM
        debug_request_t debug;
        if (xQueueReceive(debug_requests,&debug,0)==pdTRUE) {
            atom_receiver_t parser={0}; atom_request_t request; atom_status_t status; bool answered=false;
            for (unsigned i=0;i<ATOM_REQUEST_SIZE;++i)
                if (atom_receiver_feed(&parser,debug.bytes[i],now_ms(),&request,&status)) {
                    respond(request,status,debug.bytes,now_ms(),debug.token); answered=true; break;
                }
            if (!answered) {
                debug_reply_t result={.token=debug.token};
                BaseType_t sent=xQueueSend(debug_replies,&result,0); configASSERT(sent==pdTRUE);
            }
        }
#endif
        receive_chunk_t chunk;
        if (atomic_exchange(&receive_gap, false)) {
            receiver.length = 0; xQueueReset(received_chunks);
            matrix_status_note_lcd_command(false);
            atomic_fetch_add(&invalid_requests, 1);
            ESP_LOGW(TAG, "I2C receive queue overflow; discarding partial requests");
        }
        if (xQueueReceive(received_chunks, &chunk, pdMS_TO_TICKS(10)) != pdTRUE) continue;
        bool discarded = false, completed = false;
        for (unsigned i = 0; i < chunk.length; ++i) {
            if (!receiver.length && chunk.bytes[i] != 0xa5) discarded = true;
            atom_request_t request; atom_status_t status;
            uint8_t candidate[ATOM_REQUEST_SIZE]={0};
            if (receiver.length==ATOM_REQUEST_SIZE-1) {
                memcpy(candidate,receiver.bytes,ATOM_REQUEST_SIZE-1); candidate[ATOM_REQUEST_SIZE-1]=chunk.bytes[i];
            }
            if (atom_receiver_feed(&receiver, chunk.bytes[i], chunk.timestamp, &request, &status)) {
                completed = true; respond(request, status,candidate,chunk.timestamp,0);
            }
        }
        if (discarded && !completed) {
            matrix_status_note_lcd_command(false);
            atomic_fetch_add(&invalid_requests, 1);
        }
    }
}
esp_err_t atom_i2c_start(void)
{
    /* The TX replacement adapter excludes the ISR on the same core. */
    if (xPortGetCoreID() != 0) return ESP_ERR_INVALID_STATE;
    boot_id = esp_random(); if (!boot_id) boot_id = 1;
    received_chunks = xQueueCreate(8, sizeof(receive_chunk_t));
    if (!received_chunks) return ESP_ERR_NO_MEM;
#if CONFIG_REMOTE_DBG_SIM
    debug_requests=xQueueCreate(1,sizeof(debug_request_t)); debug_replies=xQueueCreate(1,sizeof(debug_reply_t));
    if (!debug_requests || !debug_replies) return ESP_ERR_NO_MEM;
#endif
    const i2c_slave_config_t config = {
        .i2c_port = I2C_NUM_0, .sda_io_num = 26, .scl_io_num = 32,
        .clk_source = I2C_CLK_SRC_DEFAULT, .slave_addr = ATOM_PROTOCOL_ADDRESS,
        .addr_bit_len = I2C_ADDR_BIT_LEN_7, .send_buf_depth = 128, .receive_buf_depth = 128,
        .intr_priority = 3,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_slave_device(&config, &slave), TAG, "create slave v2");
    const i2c_slave_event_callbacks_t callbacks = {.on_receive = receive_callback};
    ESP_RETURN_ON_ERROR(i2c_slave_register_event_callbacks(slave, &callbacks, NULL), TAG, "register receive");
    /* A 35-byte response needs ISR refill beyond the 32-byte hardware FIFO.
     * Both reply preparation and refill must preempt high-rate HID processing. */
    if (xTaskCreatePinnedToCore(i2c_task, "atom_i2c", 3072, NULL, configMAX_PRIORITIES - 1, NULL, 0) != pdPASS)
        return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Protocol v2 / new slave ready boot_id=%lu", (unsigned long)boot_id);
    return ESP_OK;
}
