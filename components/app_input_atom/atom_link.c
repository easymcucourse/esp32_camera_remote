#include "input_atom.h"
#include "pad_types.h"
#include <stdatomic.h>
#include "atom_client.h"
#include "i2c_monitor.h"
#include "app_console.h"
#include "input_provider.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "atom_link";
static input_provider_handle_t handle;
static atomic_bool running,stopping;
static i2c_master_bus_handle_t prepared_bus;
static i2c_master_dev_handle_t prepared_device;
static uint32_t epoch=1,id;
static bool mode_known;
static bool edge_valid, report_gap;
static uint32_t edge_buttons;
static bool sim_active;
static uint8_t source_tag, gimbal_state;
static unsigned observed_mode;
static i2c_monitor_t monitor;
static void commands(void)
{
    app_message_t m;
    for (unsigned i=0;i<4 && app_console_receive(APP_ENDPOINT_INPUT_ATOM,&m,0)==ESP_OK;++i) {
        app_message_t reply={.result=ESP_ERR_INVALID_ARG};
        bool valid=m.type==APP_MESSAGE_INPUT_ATOM_COMMAND && m.target==APP_ENDPOINT_INPUT_ATOM && !m.lease && m.generation &&
            m.generation==app_console_endpoint_generation(APP_ENDPOINT_INPUT) &&
            m.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_INPUT_ATOM) &&
            m.deadline_us>esp_timer_get_time();
        if (valid && m.source==APP_ENDPOINT_INPUT && !m.flags &&
            m.payload.command.index==APP_INPUT_PROVIDER_PAD_KIND &&
            m.payload.command.value<=1) {
            observed_mode=m.payload.command.value;mode_known=true;
            reply.result=ESP_OK;
        } else if (m.type==APP_MESSAGE_INPUT_ATOM_COMMAND && m.target==APP_ENDPOINT_INPUT_ATOM && !m.lease &&
            m.source==APP_ENDPOINT_UART && m.flags==APP_MESSAGE_REQUEST && m.generation &&
            m.generation==app_console_endpoint_generation(APP_ENDPOINT_UART) &&
            m.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_INPUT_ATOM) &&
            m.deadline_us>esp_timer_get_time()) {
            unsigned op=m.payload.command.index;
            if (op==APP_INPUT_ATOM_LOG_MODE && m.payload.command.value<=I2C_LOG_CHANGES) {
                i2c_monitor_mode(&monitor,(i2c_log_mode_t)m.payload.command.value);reply.result=ESP_OK;
            } else if (op==APP_INPUT_ATOM_STATS) {
                if (m.payload.command.flag) i2c_monitor_reset_stats(&monitor);
                reply.payload.i2c_stats=monitor.stats;reply.result=ESP_OK;
            } else if (op==APP_INPUT_ATOM_LOG_READ) {
                reply.payload.i2c.available=i2c_monitor_read(&monitor,&reply.payload.i2c.frame);
                reply.payload.i2c.dropped=monitor.stats.log_dropped;reply.result=ESP_OK;
            }
        }
        if (m.flags==APP_MESSAGE_REQUEST) app_console_reply(&m,&reply);
        app_message_release(&m);
    }
}
static void link_wait(unsigned ms)
{
    TickType_t started=xTaskGetTickCount(),duration=pdMS_TO_TICKS(ms);
    while (!atomic_load(&stopping) && (TickType_t)(xTaskGetTickCount()-started)<duration) {
        unsigned previous=observed_mode;commands();
        if (previous!=observed_mode) return;
        TickType_t remaining=duration-(TickType_t)(xTaskGetTickCount()-started);
        TickType_t slice=remaining<pdMS_TO_TICKS(25) ? remaining : pdMS_TO_TICKS(25);
        if (ulTaskNotifyTake(pdTRUE,slice)) return;
    }
}
static void publish_status(const atom_client_t *client, const gamepad_snapshot_t *pad)
{
    if (id==UINT32_MAX) {
        input_provider_disconnect(handle,INPUT_DISCONNECT_RESTART);
        if (epoch==UINT32_MAX) { atomic_store(&stopping,true);return; }
        ++epoch;id=0;
    }
    input_report_t report={.atom_online=client->online,.connected=pad->connected,
        .mismatch=client->mismatch,.sim=sim_active,.buttons=pad->buttons,
        .rx=pad->rx,.ry=pad->ry,.lt=pad->lt,.rt=pad->rt,.battery=pad->battery,
        .kind=client->input_mode,.gimbal=gimbal_state,.source_epoch=epoch,.report_id=++id,
        .gap=report_gap,.event_valid=edge_valid,.event_buttons=edge_buttons};
    input_provider_publish(handle,&report);
    edge_valid=report_gap=false;

}
static uint32_t input_now_ms(void)
{
    return (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS);
}
static void input_offline(void)
{
    input_provider_disconnect(handle,INPUT_DISCONNECT_OFFLINE);
}
static void source_restart(void)
{
    input_offline();
    if (epoch!=UINT32_MAX) { ++epoch;id=0; }
    else atomic_store(&stopping,true);
    edge_valid=report_gap=false;
}

static void report_buttons(uint32_t previous, uint32_t buttons)
{
    static const char *const names[] = {
        "Share", "L3", "R3", "Options", "Up", "Right", "Down", "Left",
        "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square", "PS", "Touchpad"
    };
    uint32_t changed = previous ^ buttons;
    for (unsigned bit = 0; bit < sizeof(names) / sizeof(names[0]); ++bit) {
        if (changed & (1UL << bit))
            ESP_LOGI(TAG, "DS4 %s %s", names[bit], buttons & (1UL << bit) ? "pressed" : "released");
    }
}

esp_err_t input_atom_prepare(void)
{
    if (atomic_load(&running)) return ESP_ERR_INVALID_STATE;
    if (prepared_device) return ESP_OK;
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device=NULL;
    esp_err_t err = i2c_master_get_bus_handle(I2C_NUM_0, &bus);
    if (err == ESP_OK) {
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ATOM_PROTOCOL_ADDRESS, .scl_speed_hz = 100000,
        };
        err = i2c_master_bus_add_device(bus, &config, &device);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize: %s", esp_err_to_name(err));
        return err;
    }
    prepared_bus=bus;prepared_device=device;return ESP_OK;
}
static void atom_link_task(void *arg)
{
    (void)arg;
    i2c_master_bus_handle_t bus=prepared_bus;
    i2c_master_dev_handle_t device=prepared_device;
    esp_err_t err;
    atom_client_t client = {0};
    gamepad_snapshot_t debug_pad = {.battery = 255};
    bool discard_cached = true;
    uint32_t event_buttons = 0, last_buttons = 0, last_log_ms = 0, last_diagnostic_ms = 0;
    while (!atomic_load(&stopping)) {
        TickType_t cycle = xTaskGetTickCount();
        commands();
        if (!mode_known) { link_wait(25);continue; }
        if (!client.online && i2c_master_probe(bus,ATOM_PROTOCOL_ADDRESS,100)!=ESP_OK) {
            input_offline();
            debug_pad = (gamepad_snapshot_t){.battery = 255}; publish_status(&client, &debug_pad);
            link_wait(client.mismatch ? 5000 : 1000); continue;
        }
        unsigned mode=observed_mode;
        if (client.input_mode!=mode) {
            source_restart();discard_cached=true;event_buttons=last_buttons=0;
            client.online=false;client.ack_id=0;client.failures=0;client.mismatch=false;client.input_mode=mode;
        }
        atom_request_t request = atom_client_request(&client);
        uint8_t command[ATOM_REQUEST_SIZE], reply[ATOM_RESPONSE_MAX];
        atom_encode_request(command, request);
        size_t size = 7 + (client.online ? ATOM_POLL_SIZE : ATOM_HELLO_SIZE);
        uint32_t transaction_started=input_now_ms();
        err=i2c_master_transmit(device,command,sizeof(command),100);
        if (err==ESP_OK && !atomic_load(&stopping)) {
            vTaskDelay(pdMS_TO_TICKS(15));
            err=i2c_master_receive(device,reply,size,100);
        } else if (err==ESP_OK) err=ESP_ERR_INVALID_STATE;
        const uint8_t *p = NULL;
        atom_client_result_t result = err == ESP_OK ? atom_client_response(&client, reply, size, &p) :
            atom_client_failure(&client);
        i2c_frame_record_t record={.timestamp=transaction_started,.elapsed_ms=input_now_ms()-transaction_started};
        memcpy(record.request,command,sizeof(command));
        if (err==ESP_OK) {
            record.response_len=size; memcpy(record.response,reply,size);
            record.result=i2c_monitor_response(reply,size,request,(uint8_t)(size-7));
            if (record.result==I2C_MON_OK && (result==ATOM_CLIENT_RETRY || result==ATOM_CLIENT_OFFLINE || result==ATOM_CLIENT_MISMATCH))
                record.result=I2C_MON_BAD_PARAM;
        } else record.result=err==ESP_ERR_TIMEOUT?I2C_MON_TIMEOUT:I2C_MON_IO;
        i2c_monitor_record(&monitor,&record);
        if (result == ATOM_CLIENT_RETRY) {
            ESP_LOGW(TAG, "Transaction failed (%u/3); retry seq=%u", client.failures, client.seq);
            if ((uint32_t)(input_now_ms() - last_diagnostic_ms) >= 1000) {
                last_diagnostic_ms = input_now_ms();
                if (err != ESP_OK) ESP_LOGW(TAG, "I2C transport: %s", esp_err_to_name(err));
                else {
                    size_t frame_size = (size_t)reply[5] + 7;
                    bool bounded = frame_size <= size;
                    ESP_LOGW(TAG, "Reply head=%02x v=%u seq=%u/%u cmd=%02x/%02x status=%u len=%u CRC=%02x/%02x bounded=%d",
                        reply[0], reply[1], reply[2], request.seq, reply[3], request.cmd, reply[4], reply[5],
                        bounded ? reply[frame_size - 1] : 0, bounded ? atom_crc8(reply, frame_size - 1) : 0, bounded);
                }
            }
        } else if (result == ATOM_CLIENT_OFFLINE || result == ATOM_CLIENT_MISMATCH || result == ATOM_CLIENT_RESTART) {
            source_restart(); discard_cached = true; event_buttons = last_buttons = 0;
            debug_pad = (gamepad_snapshot_t){.battery = 255};
            gimbal_state=0;
            ESP_LOGW(TAG, "%s", result == ATOM_CLIENT_RESTART ? "ATOM restarted; releasing old input" :
                     client.mismatch ? "ATOM firmware version mismatch / HELLO failed" : "ATOM link lost");
            if (result != ATOM_CLIENT_RESTART) {
                publish_status(&client,&debug_pad);
                link_wait(client.mismatch ? 5000 : 1000);
            }
        } else if (result == ATOM_CLIENT_HELLO) {
            source_restart(); discard_cached = true; event_buttons = last_buttons = 0;
            debug_pad = (gamepad_snapshot_t){.battery = 255};
            gimbal_state=0;
            ESP_LOGI(TAG, "ATOM v2 online boot_id=%lu local_mask=0x%06lx",
                     (unsigned long)client.boot_id, (unsigned long)client.local_mask);
        } else if (result == ATOM_CLIENT_POLL) {
            bool simulated=(p[27]&ATOM_DEBUG_SIM)!=0;
            if (p[27]!=source_tag || simulated!=sim_active) {
                source_restart(); discard_cached=true; event_buttons=last_buttons=0;
                source_tag=p[27]; sim_active=simulated;
                ESP_LOGW(TAG,"Input source changed: SIM=%d; previous input released",sim_active);
            }
            bool online = p[4] == 3;
            uint32_t buttons = online ? atom_read_le(p + 11, 3) : 0;
            gamepad_snapshot_t snapshot = {
                .connected = online, .buttons = buttons, .rx = (int8_t)p[14], .ry = (int8_t)p[15],
                .lt = p[16], .rt = p[17], .battery = p[8],
            };
            debug_pad = snapshot;
            gimbal_state=p[5];
            bool valid = (p[18] & 1) != 0;
            report_gap=discard_cached || (p[18]&2)!=0;
            if (valid) {
                uint32_t id=atom_read_le(p+19,4);
                if (id!=client.ack_id) {
                    uint32_t cached=atom_read_le(p+23,3);
                    if (online) report_buttons(event_buttons,cached);
                    edge_valid=online && !discard_cached;edge_buttons=cached;
                    event_buttons=cached;client.ack_id=id;
                }
            }
            if (online && discard_cached && !valid) {
                discard_cached=false;event_buttons=buttons;
            }
            if (online && (buttons != last_buttons || (uint32_t)(input_now_ms() - last_log_ms) >= 1000)) {
                ESP_LOGI(TAG, "Controller buttons=0x%05lx R=(%d,%d) LT=%u RT=%u battery=%u fault=0x%02x pending=%u",
                    (unsigned long)buttons, snapshot.rx, snapshot.ry, snapshot.lt, snapshot.rt,
                    snapshot.battery, p[7], p[26]);
                last_log_ms = input_now_ms();
            }
            last_buttons = buttons;
        }
        publish_status(&client, &debug_pad);
        TickType_t elapsed=xTaskGetTickCount()-cycle;
        if (elapsed<pdMS_TO_TICKS(50)) link_wait((pdMS_TO_TICKS(50)-elapsed)*portTICK_PERIOD_MS);
    }
    input_offline();input_provider_unregister(handle);handle=0;
    /* Keep the sole transport owner alive if device removal fails. Core may
     * time out, but may not restart over an unreturned I2C device handle. */
    while (i2c_master_bus_rm_device(device)!=ESP_OK) vTaskDelay(pdMS_TO_TICKS(10));
    prepared_device=NULL;prepared_bus=NULL;
    app_console_endpoint_stop(APP_ENDPOINT_INPUT_ATOM);
    atomic_store(&running,false);vTaskDelete(NULL);
}

esp_err_t input_atom_start(void)
{
    if (atomic_load(&running)) return ESP_ERR_INVALID_STATE;
    esp_err_t prepared=input_atom_prepare();
    if(prepared!=ESP_OK)return prepared;
    const app_endpoint_config_t endpoint={4,1};
    esp_err_t err=app_console_endpoint_register(APP_ENDPOINT_INPUT_ATOM,&endpoint);
    if (err!=ESP_OK) return err;
    err=input_provider_register(INPUT_SOURCE_ATOM,&handle);
    if (err!=ESP_OK) { app_console_endpoint_stop(APP_ENDPOINT_INPUT_ATOM);return err; }
    mode_known=false;observed_mode=0;epoch=1;id=0;source_tag=gimbal_state=0;
    sim_active=edge_valid=report_gap=false;
    monitor=(i2c_monitor_t){0};
    atomic_store(&stopping,false);atomic_store(&running,true);
    if (xTaskCreate(atom_link_task,"atom_link",3072,NULL,4,NULL)!=pdPASS) {
        input_provider_unregister(handle);handle=0;app_console_endpoint_stop(APP_ENDPOINT_INPUT_ATOM);
        atomic_store(&running,false);return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t input_atom_stop(uint32_t timeout_ms)
{
    atomic_store(&stopping,true);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&running)) {
        if (esp_timer_get_time()>=deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(prepared_device) {
        esp_err_t error=i2c_master_bus_rm_device(prepared_device);
        if(error!=ESP_OK)return error;
        prepared_device=NULL;prepared_bus=NULL;
    }
    return ESP_OK;
}
