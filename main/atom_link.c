#include "atom_link.h"
#include "atom_client.h"
#include "i2c_debug.h"
#include "lcd_sim.h"
#include "ui_preferences.h"
#include "board_7b.h"
#include "camera_pair.h"
#include "gamepad_input.h"
#include "wifi_menu_ui.h"
#include "maint_mode.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "atom_link";
static gamepad_input_t gamepad;
static portMUX_TYPE status_mux = portMUX_INITIALIZER_UNLOCKED;
static atom_link_status_t status;
static bool sim_active;
static uint8_t source_tag;
static TaskHandle_t link_task;
void atom_link_wake(void) { if (link_task) xTaskNotifyGive(link_task); }
static void link_wait(unsigned ms) { ulTaskNotifyTake(pdTRUE,pdMS_TO_TICKS(ms)); }
void atom_link_get_status(atom_link_status_t *out)
{ portENTER_CRITICAL(&status_mux); *out = status; portEXIT_CRITICAL(&status_mux); }
static void publish_status(const atom_client_t *client, const gamepad_snapshot_t *pad)
{ portENTER_CRITICAL(&status_mux); status.client = *client; status.pad = *pad; status.sim=sim_active; portEXIT_CRITICAL(&status_mux); }
static uint32_t input_now_ms(void)
{
    return (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS);
}
static bool input_action(void *context, pad_action_t action)
{
    (void)context;
    if (!wifi_menu_ui_active()) {
        bool requested=action.type==PAD_ACTION_MAINT_TOGGLE ||
            (board_7b_settings_mode() && board_7b_menu_selected()==8 &&
             (action.type==PAD_ACTION_MENU_CONFIRM || action.type==PAD_ACTION_MENU_STEP));
        bool accepted=maint_mode_gamepad(action);
        if (requested) return accepted; /* Queue failure reaches input safety cancellation. */
    }
    if (action.type==PAD_ACTION_UI_INFO_NEXT) {
        uint32_t token;esp_err_t err=ui_preferences_request(0,true,&token);
        if (err!=ESP_OK) ESP_LOGW(TAG,"INFO request rejected: %s",esp_err_to_name(err));
        return err==ESP_OK;
    }
    if (action.type == PAD_ACTION_UI_TOGGLE) {
        bool settings = board_7b_toggle_settings_mode();
        ESP_LOGI(TAG, "DS4 Start/Options: %s screen", settings ? "settings" : "preview");
        return true;
    }
    if (action.type == PAD_ACTION_MENU_MOVE || action.type == PAD_ACTION_MENU_CONFIRM ||
        action.type == PAD_ACTION_MENU_BACK || (action.type == PAD_ACTION_MENU_STEP &&
        (wifi_menu_ui_active() || board_7b_menu_selected() == 7))) return wifi_menu_ui_action(action);
    if (action.type == PAD_ACTION_RELEASE_ALL && wifi_menu_ui_active()) wifi_menu_ui_action(action);
    if (action.type == PAD_ACTION_MENU_STEP) {
        gamepad_caps_t caps; camera_gamepad_caps(&caps);
        if (!caps.session) return true; /* Camera rows remain read-only while offline. */
    }
    return camera_gamepad_action(action);
}
static void input_offline(void)
{
    gamepad_input_offline(&gamepad);
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

static void atom_link_task(void *arg)
{
    (void)arg;
    gamepad_input_init(&gamepad, input_action, NULL);
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
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
        vTaskDelete(NULL); return;
    }
    atom_client_t client = {0};
    gamepad_snapshot_t debug_pad = {.battery = 255};
    bool discard_cached = true;
    uint32_t event_buttons = 0, last_buttons = 0, last_log_ms = 0, last_diagnostic_ms = 0;
    uint32_t local_epoch=lcd_sim_epoch();
    for (;;) {
        TickType_t cycle = xTaskGetTickCount();
        bool local=lcd_sim_enabled();
        if (local_epoch!=lcd_sim_epoch()) {
            local_epoch=lcd_sim_epoch();input_offline();client=(atom_client_t){0};
            discard_cached=true;event_buttons=last_buttons=0;source_tag=0;
            sim_active=local;board_7b_set_sim(local);board_7b_set_atom_status(false,false);
            debug_pad=(gamepad_snapshot_t){.battery=255};publish_status(&client,&debug_pad);
            ESP_LOGW(TAG,"SIM local transport=%d; previous input released",local);
        }
        if ((local && !lcd_sim_online()) ||
            (!local && !client.online && i2c_master_probe(bus, ATOM_PROTOCOL_ADDRESS, 100) != ESP_OK)) {
            sim_active=local; board_7b_set_sim(local);
            if (local) { client.online=false;client.mismatch=false;client.ack_id=0;client.failures=0; }
            input_offline(); board_7b_set_atom_status(false, false);
            debug_pad = (gamepad_snapshot_t){.battery = 255}; publish_status(&client, &debug_pad);
            link_wait(client.mismatch ? 5000 : 1000); continue;
        }
        unsigned mode=ui_preferences_pad();
        if (client.input_mode!=mode) {
            input_offline();discard_cached=true;event_buttons=last_buttons=0;
            client.online=false;client.ack_id=0;client.failures=0;client.mismatch=false;client.input_mode=mode;
        }
        atom_request_t request = atom_client_request(&client);
        uint8_t command[ATOM_REQUEST_SIZE], reply[ATOM_RESPONSE_MAX];
        atom_encode_request(command, request);
        size_t size = 7 + (client.online ? ATOM_POLL_SIZE : ATOM_HELLO_SIZE);
        uint32_t transaction_started=input_now_ms();
        if (local) err=lcd_sim_transact(command,reply,&size);
        else {
            err = i2c_master_transmit(device, command, sizeof(command), 100);
            if (err == ESP_OK) {
                vTaskDelay(pdMS_TO_TICKS(15));
                err = i2c_master_receive(device, reply, size, 100);
            }
        }
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
        if (!local) i2c_debug_record(&record);
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
            sim_active=local; board_7b_set_sim(local);
            input_offline(); discard_cached = true; event_buttons = last_buttons = 0;
            debug_pad = (gamepad_snapshot_t){.battery = 255};
            board_7b_set_atom_status(false, false);
            board_7b_set_atom_protocol(client.mismatch, 0);
            ESP_LOGW(TAG, "%s", result == ATOM_CLIENT_RESTART ? "ATOM restarted; releasing old input" :
                     client.mismatch ? "ATOM firmware version mismatch / HELLO failed" : "ATOM link lost");
            if (result != ATOM_CLIENT_RESTART) link_wait(client.mismatch ? 5000 : 1000);
        } else if (result == ATOM_CLIENT_HELLO) {
            input_offline(); discard_cached = true; event_buttons = last_buttons = 0;
            debug_pad = (gamepad_snapshot_t){.battery = 255};
            board_7b_set_atom_status(true, false); board_7b_set_atom_protocol(false, 0);
            ESP_LOGI(TAG, "ATOM v2 online boot_id=%lu local_mask=0x%06lx",
                     (unsigned long)client.boot_id, (unsigned long)client.local_mask);
        } else if (result == ATOM_CLIENT_POLL) {
            bool simulated=(p[27]&ATOM_DEBUG_SIM)!=0;
            if (p[27]!=source_tag || simulated!=sim_active) {
                input_offline(); discard_cached=true; event_buttons=last_buttons=0;
                source_tag=p[27]; sim_active=simulated; board_7b_set_sim(sim_active);
                ESP_LOGW(TAG,"Input source changed: SIM=%d; previous input released",sim_active);
            }
            bool online = p[4] == 3;
            uint32_t buttons = online ? atom_read_le(p + 11, 3) : 0;
            gamepad_snapshot_t snapshot = {
                .connected = online, .buttons = buttons, .rx = (int8_t)p[14], .ry = (int8_t)p[15],
                .lt = p[16], .rt = p[17], .battery = p[8],
            };
            debug_pad = snapshot;
            gamepad_caps_t caps; camera_gamepad_caps(&caps);
            caps.settings = board_7b_settings_mode();
            if (online && !gamepad.connected) {
                gamepad_input_online(&gamepad, &snapshot, &caps);
                discard_cached = true; event_buttons = buttons;
            }
            if (!online && gamepad.connected) input_offline();
            board_7b_set_atom_status(true, online); board_7b_set_atom_protocol(false, p[5]);
            bool valid = (p[18] & 1) != 0;
            if (valid) {
                uint32_t id = atom_read_le(p + 19, 4);
                if (id != client.ack_id) {
                    uint32_t cached = atom_read_le(p + 23, 3);
                    if (online) {
                        report_buttons(event_buttons, cached);
                        gamepad_input_event(&gamepad, cached, discard_cached || (p[18] & 2), &caps, input_now_ms());
                    }
                    event_buttons = cached; client.ack_id = id;
                }
            }
            if (online && discard_cached && !valid) {
                camera_gamepad_caps(&caps);
                caps.settings = board_7b_settings_mode();
                gamepad_input_event(&gamepad, buttons, true, &caps, input_now_ms());
                discard_cached = false; event_buttons = buttons;
            }
            camera_gamepad_caps(&caps);
            caps.settings = board_7b_settings_mode();
            gamepad_input_snapshot(&gamepad, &snapshot, &caps, input_now_ms());
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
}

void atom_link_start(void)
{
    ESP_ERROR_CHECK(lcd_sim_start());
    if (xTaskCreate(atom_link_task, "atom_link", 3072, NULL, 4, &link_task) != pdPASS)
        ESP_LOGE(TAG, "could not create communication task");
}
