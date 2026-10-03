#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "atom_i2c.h"
#include "matrix_status.h"
#include "ds4_host.h"
#include "atom_console.h"

#define ATOM_BUTTON_GPIO GPIO_NUM_39
static const char *TAG = "atom_matrix";

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
    ESP_ERROR_CHECK(matrix_status_init());
    button_init();
    ESP_ERROR_CHECK(atom_i2c_start());
    matrix_status_boot_stage(MATRIX_BOOT_STORAGE);
    ESP_ERROR_CHECK(ds4_host_init());
    atom_i2c_ready();
    atom_console_start();
    ESP_LOGI(TAG, "ATOM ready; I2C v2 and Matrix status renderer enabled");

    bool stable = false, candidate = false;
    uint32_t changed_ms = 0;
    uint16_t press_count = 0;
    ds4_state_t last_ds4 = {0};
    TickType_t last_log = 0;
    for (;;) {
        ds4_state_t ds4; ds4_host_get_state(&ds4);
        TickType_t ticks = xTaskGetTickCount();
        if (ds4.connected && (ds4.buttons != last_ds4.buttons || ticks - last_log >= pdMS_TO_TICKS(1000))) {
            ESP_LOGI(TAG, "%s buttons=0x%05lx L=(%d,%d) R=(%d,%d) LT=%u RT=%u battery=%u",
                ds4_host_sim_active()?"SIM":"Gamepad", (unsigned long)ds4.buttons, ds4.lx, ds4.ly, ds4.rx, ds4.ry, ds4.l2, ds4.r2, ds4.battery);
            last_log = ticks;
        }
        last_ds4 = ds4;
        uint32_t now = (uint32_t)((uint64_t)ticks * portTICK_PERIOD_MS);
        bool pressed = gpio_get_level(ATOM_BUTTON_GPIO) == 0;
        if (pressed != candidate) { candidate = pressed; changed_ms = now; }
        if (candidate != stable && (uint32_t)(now - changed_ms) >= 30) {
            stable = candidate; if (stable) ++press_count;
            atom_i2c_button(stable, press_count);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
