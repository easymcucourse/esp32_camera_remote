#include "atom_link.h"
#include "board_7b.h"
#include "camera_pair.h"
#include "focus_input.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "atom_link";
static focus_input_t focus_input;

static void focus_buttons(uint32_t buttons, bool online, bool event)
{
    bool eligible = online && camera_focus_ready();
    uint32_t keys = buttons & FOCUS_KEYS;
    if (!eligible || !keys || keys == FOCUS_KEYS ||
        (focus_input.held && keys != focus_input.held)) camera_focus_cancel();
    uint32_t now = (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS);
    int direction = focus_input_update(&focus_input, buttons, eligible, event, now);
    if (direction) camera_focus_step(direction);
}
enum {
    ATOM_ADDRESS = 0x42,
    ATOM_CMD_RGB = 1,
    ATOM_CMD_EVENTS = 3,
    ATOM_LEGACY_REPLY_SIZE = 8,
    ATOM_EVENT_REPLY_SIZE = 26,
    DS4_START = 1UL << 3,
    DS4_L1 = 1UL << 10,
    DS4_R1 = 1UL << 11,
};

static void report_buttons(uint32_t previous, uint32_t buttons)
{
    static const char *const names[] = {
        "Share", "L3", "R3", "Options", "Up", "Right", "Down", "Left",
        "L2", "R2", "L1", "R1", "Triangle", "Circle", "Cross", "Square", "PS", "Touchpad"
    };
    uint32_t changed = previous ^ buttons;
    uint32_t pressed = buttons & ~previous;
    // Simultaneous shoulders cancel; buffered edges trigger only once.
    if ((pressed & (DS4_L1 | DS4_R1)) == DS4_L1) camera_mode_step(-1);
    if ((pressed & (DS4_L1 | DS4_R1)) == DS4_R1) camera_mode_step(1);
    // DS4 Options is the Start button. Toggle only on its rising edge.
    if (pressed & DS4_START) {
        bool settings = board_7b_toggle_settings_mode();
        camera_focus_cancel();
        ESP_LOGI(TAG, "DS4 Start/Options: %s screen", settings ? "settings" : "preview");
    }
    for (unsigned bit = 0; bit < sizeof(names) / sizeof(names[0]); ++bit) {
        if (changed & (1UL << bit))
            ESP_LOGI(TAG, "DS4 %s %s", names[bit], buttons & (1UL << bit) ? "pressed" : "released");
    }
}

static uint32_t read_u24(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) | ((uint32_t)bytes[2] << 16);
}

static void atom_link_task(void *arg)
{
    i2c_master_bus_handle_t bus;
    i2c_master_dev_handle_t device;
    esp_err_t err = i2c_master_get_bus_handle(I2C_NUM_0, &bus);
    if (err == ESP_OK) {
        const i2c_device_config_t config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = ATOM_ADDRESS,
            .scl_speed_hz = 100000,
        };
        err = i2c_master_bus_add_device(bus, &config, &device);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "initialize: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }
    bool connected = false;
    uint8_t sequence = 0;
    uint16_t last_count = 0;
    uint8_t last_button = 0;
    bool controller_connected = false;
    uint32_t last_buttons = 0;
    uint32_t event_ack = 0, event_buttons = 0;
    TickType_t last_input_log = 0;
    for (;;) {
        if (!connected && i2c_master_probe(bus, ATOM_ADDRESS, 100) != ESP_OK) {
            connected = false;
            if (controller_connected) ESP_LOGW(TAG, "DS4 disconnected (ATOM unavailable)");
            controller_connected = false;
            focus_buttons(0, false, false);
            last_buttons = 0;
            board_7b_set_atom_status(false, false);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        /* A separate read allows the ESP32 slave task to prepare its reply. */
        const uint8_t command[8] = {0xA5, 1, ++sequence,
            connected ? ATOM_CMD_EVENTS : ATOM_CMD_RGB,
            connected ? event_ack & 0xff : 0,
            connected ? (event_ack >> 8) & 0xff : 16,
            connected ? (event_ack >> 16) & 0xff : 0,
            connected ? event_ack >> 24 : 0};
        uint8_t reply[ATOM_EVENT_REPLY_SIZE];
        size_t reply_length = command[3] == ATOM_CMD_EVENTS ? sizeof(reply) : ATOM_LEGACY_REPLY_SIZE;
        err = i2c_master_transmit(device, command, sizeof(command), 100);
        vTaskDelay(pdMS_TO_TICKS(30));
        if (err == ESP_OK) err = i2c_master_receive(device, reply, reply_length, 100);
        if (err == ESP_OK && reply[0] == 0x5A && reply[1] == 1 &&
            reply[2] == sequence && reply[3] == 0) {
            const uint16_t count = reply[5] | ((uint16_t)reply[6] << 8);
            if (!connected || count != last_count || reply[4] != last_button) {
                ESP_LOGI(TAG, "ATOM online: button=%u presses=%u", reply[4], count);
            }
            connected = true;
            board_7b_set_atom_status(true, reply[7] != 0);
            bool online = reply[7] != 0;
            if (online != controller_connected)
                ESP_LOGI(TAG, "DS4 %s", online ? "connected" : "disconnected");
            controller_connected = online;
            if (reply_length == sizeof(reply)) {
                uint32_t buttons = online ? read_u24(reply + 8) : 0;
                if (reply[18] == 1) {
                    uint32_t id = (uint32_t)reply[19] | ((uint32_t)reply[20] << 8) |
                        ((uint32_t)reply[21] << 16) | ((uint32_t)reply[22] << 24);
                    if (id && id != event_ack) {
                        uint32_t cached = read_u24(reply + 23);
                        if (online) {
                            report_buttons(event_buttons, cached);
                            focus_buttons(cached, true, true);
                        }
                        event_buttons = cached;
                        event_ack = id;
                        ESP_LOGI(TAG, "DS4 cached event=%lu", (unsigned long)id);
                    }
                }
                TickType_t now = xTaskGetTickCount();
                focus_buttons(buttons, online, false);
                if (online && (buttons != last_buttons ||
                    now - last_input_log >= pdMS_TO_TICKS(1000))) {
                    ESP_LOGI(TAG, "DS4 buttons=0x%05lx L=(%d,%d) R=(%d,%d) L2=%u R2=%u battery=%u",
                             (unsigned long)buttons, (int8_t)reply[11], (int8_t)reply[12],
                             (int8_t)reply[13], (int8_t)reply[14], reply[15], reply[16], reply[17]);
                    last_input_log = now;
                }
                last_buttons = buttons;
            }
            last_count = count;
            last_button = reply[4];
        } else {
            if (connected) ESP_LOGW(TAG, "ATOM response invalid or timed out");
            connected = false;
            if (controller_connected) ESP_LOGW(TAG, "DS4 disconnected (ATOM link lost)");
            controller_connected = false;
            focus_buttons(0, false, false);
            last_buttons = 0;
            board_7b_set_atom_status(false, false);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void atom_link_start(void)
{
    if (xTaskCreate(atom_link_task, "atom_link", 3072, NULL, 4, NULL) != pdPASS)
        ESP_LOGE(TAG, "could not create communication task");
}
