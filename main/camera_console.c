#include "camera_console.h"
#include "camera_pair.h"
#include "board_7b.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "camera_pair";

static void console_task(void *unused)
{
    while (true) {
        uint8_t command;
        if (uart_read_bytes(UART_NUM_0, &command, 1, pdMS_TO_TICKS(1000)) == 1) {
            if (command == 'p' || command == 'P') camera_pair_start();
            if (command == 'j' || command == 'J') camera_jpeg_start();
            if (command == 'u') camera_forget_pairing();
            if (command == 'S') {
                bool enabled = board_7b_toggle_settings_mode();
                camera_focus_cancel();
                ESP_LOGI(TAG, "Settings display %s: preview=%s",
                         enabled ? "enabled" : "disabled",
                         enabled ? "768x432" : "1024x576");
            }
            if (command == 's') {
                camera_stop_request();
            }
        }
    }
}

void camera_console_init(void)
{
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(xTaskCreate(console_task, "pair_console", 4096, NULL, 3, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "UART: j = live-view, S = settings display, s = stop, p = pairing, u = forget pairing (idle only)");
}
