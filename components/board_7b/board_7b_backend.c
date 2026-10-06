#include "display_backend.h"
#include "board_lcd.h"
#include "board_dimensions.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "board_7b";
static i2c_master_bus_handle_t bus;
static i2c_master_dev_handle_t expander;
static esp_err_t write_register(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    esp_err_t err = ESP_FAIL;
    for (unsigned attempt = 1; attempt <= 3; ++attempt) {
        err = i2c_master_transmit(expander, data, sizeof(data), 100);
        if (err == ESP_OK) return err;
        ESP_LOGW(TAG, "Expander register 0x%02x retry %u/3: %s", reg, attempt, esp_err_to_name(err));
        if (attempt < 3) vTaskDelay(pdMS_TO_TICKS(20));
    }
    return err;
}

esp_err_t display_backend_init(display_backend_prepare_t prepare, esp_err_t (*prepare_resources)(void))
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = 8,
        .scl_io_num = 9,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_config, &bus), TAG, "I2C bus");
    // 7B uses a register-based expander at 0x24. Do not use the 7/CH422G protocol.
    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x24,
        .scl_speed_hz = 100000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &device_config, &expander), TAG, "expander");
    ESP_RETURN_ON_ERROR(write_register(0x02, 0xff), TAG, "expander output mode");
    // EXIO6 LCD power high, EXIO4 SD CS high, EXIO5 USB select low.
    // Keep backlight (EXIO2) off until the framebuffer has been initialized.
    const uint8_t outputs = 0xff & ~(1 << 2) & ~(1 << 5);
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs), TAG, "LCD power");
    ESP_RETURN_ON_ERROR(write_register(0x05, 0), TAG, "backlight PWM");
    vTaskDelay(pdMS_TO_TICKS(100));

    if (prepare_resources) ESP_RETURN_ON_ERROR(prepare_resources(), TAG, "display resources");
    ESP_RETURN_ON_ERROR(board_lcd_init(prepare), TAG, "RGB panel");
    ESP_RETURN_ON_ERROR(write_register(0x03, outputs | (1 << 2)), TAG, "backlight on");
    return ESP_OK;
}

bool display_backend_ready(void) { return board_lcd_ready(); }
void display_backend_dimensions(size_t *width, size_t *height, size_t *stride)
{ *width = BOARD_LCD_WIDTH; *height = BOARD_LCD_HEIGHT; *stride = BOARD_LCD_WIDTH; }
uint16_t *display_backend_back_buffer(void) { return board_lcd_back_buffer(); }
esp_err_t display_backend_publish(uint16_t *pixels) { return board_lcd_publish(pixels); }
esp_err_t display_backend_test_fault(unsigned mode) { return board_lcd_test_fault(mode); }
esp_err_t display_backend_recover(display_backend_prepare_t prepare)
{
    const uint8_t outputs = 0xff & ~(1 << 2) & ~(1 << 5);
    esp_err_t backlight = write_register(0x03, outputs);
    if (backlight != ESP_OK) ESP_LOGW(TAG, "Recovery backlight off: %s", esp_err_to_name(backlight));
    esp_err_t err = board_lcd_recover(prepare);
    if (err == ESP_OK) {
        backlight = write_register(0x03, outputs | (1 << 2));
        if (backlight != ESP_OK) ESP_LOGW(TAG, "Recovery backlight on: %s", esp_err_to_name(backlight));
    }
    return err;
}
