#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ds4_host.h"

#define ATOM_LED_GPIO          GPIO_NUM_27
#define ATOM_BUTTON_GPIO       GPIO_NUM_39
#define GROVE_I2C_SDA_GPIO     GPIO_NUM_26
#define GROVE_I2C_SCL_GPIO     GPIO_NUM_32
#define GROVE_I2C_FREQ_HZ      100000
#define ATOM_LED_COUNT         25
#define RMT_RESOLUTION_HZ      10000000
#define BUTTON_DEBOUNCE_MS     30

static const char *TAG = "atom_matrix";

typedef struct {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} rgb_t;

static rmt_channel_handle_t led_channel;
static rmt_encoder_handle_t led_encoder;
static uint8_t receive_buffer[8];
static size_t receive_length;
static rgb_t frame[ATOM_LED_COUNT];

static esp_err_t grove_i2c_init(void)
{
    const i2c_config_t bus_config = {
        .mode = I2C_MODE_SLAVE,
        .sda_io_num = GROVE_I2C_SDA_GPIO,
        .scl_io_num = GROVE_I2C_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .slave = {.slave_addr = 0x42, .addr_10bit_en = 0,
                  .maximum_speed = GROVE_I2C_FREQ_HZ},
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(I2C_NUM_0, &bus_config), TAG, "configure I2C slave");
    ESP_RETURN_ON_ERROR(i2c_driver_install(I2C_NUM_0, I2C_MODE_SLAVE, 256, 256, 0),
                        TAG, "install I2C slave");
    ESP_LOGI(TAG, "Grove I2C slave ready: address=0x42 SDA=26 SCL=32");
    return ESP_OK;
}

static esp_err_t matrix_init(void)
{
    const rmt_tx_channel_config_t channel_config = {
        .gpio_num = ATOM_LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&channel_config, &led_channel), TAG,
                        "create RMT channel");

    const rmt_bytes_encoder_config_t encoder_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 3,
            .level1 = 0,
            .duration1 = 9,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 9,
            .level1 = 0,
            .duration1 = 3,
        },
        .flags.msb_first = 1,
    };
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&encoder_config, &led_encoder), TAG,
                        "create LED encoder");
    return rmt_enable(led_channel);
}

static esp_err_t matrix_show(void)
{
    uint8_t grb[ATOM_LED_COUNT * 3];
    for (size_t i = 0; i < ATOM_LED_COUNT; ++i) {
        grb[i * 3] = frame[i].green;
        grb[i * 3 + 1] = frame[i].red;
        grb[i * 3 + 2] = frame[i].blue;
    }

    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    ESP_RETURN_ON_ERROR(rmt_transmit(led_channel, led_encoder, grb, sizeof(grb),
                                     &transmit_config), TAG, "transmit LED frame");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(led_channel, pdMS_TO_TICKS(100)), TAG,
                        "wait for LED frame");
    /* WS2812 latches after the data line stays low for at least 50 us. */
    esp_rom_delay_us(80);
    return ESP_OK;
}

static void matrix_fill(rgb_t color)
{
    for (size_t i = 0; i < ATOM_LED_COUNT; ++i) {
        frame[i] = color;
    }
}

static void button_init(void)
{
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << ATOM_BUTTON_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&config));
}

void app_main(void)
{
    static const rgb_t colors[] = {
        {.red = 16, .green = 0, .blue = 0},
        {.red = 0, .green = 16, .blue = 0},
        {.red = 0, .green = 0, .blue = 16},
        {.red = 12, .green = 12, .blue = 12},
        {.red = 0, .green = 0, .blue = 0},
    };
    size_t color_index = 0;

    button_init();
    ESP_ERROR_CHECK(matrix_init());
    matrix_fill(colors[color_index]);
    ESP_ERROR_CHECK(matrix_show());
    ESP_ERROR_CHECK(grove_i2c_init());
    ESP_ERROR_CHECK(ds4_host_init());
    ESP_LOGI(TAG, "M5Stack ATOM Matrix ready; LCD master can read button and set RGB");

    bool previous_pressed = false;
    uint16_t press_count = 0;
    ds4_state_t last_ds4 = {0};
    TickType_t last_ds4_log = 0;
    uint32_t last_dropped = 0;
    while (true) {
        ds4_state_t ds4;
        ds4_host_get_state(&ds4);
        if (ds4.connected && (ds4.buttons != last_ds4.buttons ||
            xTaskGetTickCount() - last_ds4_log >= pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "DS4 buttons=0x%05lx L=(%d,%d) R=(%d,%d) L2=%u R2=%u battery=%u",
                     (unsigned long)ds4.buttons, ds4.lx, ds4.ly, ds4.rx, ds4.ry,
                     ds4.l2, ds4.r2, ds4.battery);
            last_ds4_log = xTaskGetTickCount();
        }
        last_ds4 = ds4;
        const bool pressed = gpio_get_level(ATOM_BUTTON_GPIO) == 0;
        if (pressed && !previous_pressed) {
            vTaskDelay(pdMS_TO_TICKS(BUTTON_DEBOUNCE_MS));
            if (gpio_get_level(ATOM_BUTTON_GPIO) == 0) {
                color_index = (color_index + 1) % (sizeof(colors) / sizeof(colors[0]));
                ++press_count;
                matrix_fill(colors[color_index]);
                ESP_ERROR_CHECK(matrix_show());
                ESP_LOGI(TAG, "color=%u", (unsigned)color_index);
            }
        }
        previous_pressed = pressed;
        uint8_t command[8];
        int received = i2c_slave_read_buffer(I2C_NUM_0, receive_buffer + receive_length,
                                           sizeof(receive_buffer) - receive_length, 0);
        if (received > 0) receive_length += received;
        if (receive_length == sizeof(receive_buffer)) {
            memcpy(command, receive_buffer, sizeof(command));
            receive_length = 0;
            uint8_t result = 0;
            if (command[0] != 0xA5 || command[1] != 1) {
                result = 1;
            } else if (command[3] == 1) {
                /* Limit brightness to the existing low-brightness range. */
                rgb_t color = {
                    .red = command[4] > 20 ? 20 : command[4],
                    .green = command[5] > 20 ? 20 : command[5],
                    .blue = command[6] > 20 ? 20 : command[6],
                };
                matrix_fill(color);
                ESP_ERROR_CHECK(matrix_show());
            } else if (command[3] != 0 && command[3] != 2 && command[3] != 3) {
                result = 2;
            }
            // Command 2 extends the legacy reply with a fresh DS4 snapshot.
            ds4_host_get_state(&ds4);
            uint8_t reply[26] = {0x5A, 1, command[2], result,
                previous_pressed, press_count & 0xff, press_count >> 8, ds4.connected,
                ds4.buttons & 0xff, (ds4.buttons >> 8) & 0xff, (ds4.buttons >> 16) & 0xff,
                (uint8_t)ds4.lx, (uint8_t)ds4.ly, (uint8_t)ds4.rx, (uint8_t)ds4.ry,
                ds4.l2, ds4.r2, ds4.battery};
            if (result == 0 && command[3] == 3) {
                uint32_t ack = (uint32_t)command[4] | ((uint32_t)command[5] << 8) |
                    ((uint32_t)command[6] << 16) | ((uint32_t)command[7] << 24);
                ds4_event_t event;
                uint32_t dropped;
                if (ds4_host_read_event(ack, &event, &dropped)) {
                    reply[18] = 1;
                    for (unsigned i = 0; i < 4; ++i) reply[19 + i] = event.id >> (8 * i);
                    for (unsigned i = 0; i < 3; ++i) reply[23 + i] = event.buttons >> (8 * i);
                }
                if (dropped != last_dropped) {
                    ESP_LOGW(TAG, "DS4 event cache overflow: dropped=%lu", (unsigned long)dropped);
                    last_dropped = dropped;
                }
            }
            const size_t reply_length = command[3] == 3 ? sizeof(reply) : command[3] == 2 ? 18 : 8;
            ESP_ERROR_CHECK(i2c_reset_tx_fifo(I2C_NUM_0));
            if (i2c_slave_write_buffer(I2C_NUM_0, reply, reply_length,
                                      pdMS_TO_TICKS(20)) != reply_length)
                ESP_LOGW(TAG, "could not queue complete reply");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
