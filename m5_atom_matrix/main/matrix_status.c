#include "matrix_status.h"
#include "ds4_host.h"
#include "matrix_debug.h"
#include "debug_console.h"
#include <string.h>
#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "matrix_status";
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static matrix_model_t state;
#if CONFIG_REMOTE_DBG_SIM
static matrix_debug_t debug;
#endif
static rmt_channel_handle_t channel;
static rmt_encoder_handle_t encoder;
/* Persistent storage remains valid even if a transmit wait times out. */
static uint8_t grb[75];
static uint32_t now_ms(void) { return (uint32_t)((uint64_t)xTaskGetTickCount() * portTICK_PERIOD_MS); }
void matrix_status_boot_stage(matrix_boot_t stage)
{ portENTER_CRITICAL(&lock); matrix_model_stage(&state, stage, now_ms()); portEXIT_CRITICAL(&lock); }
void matrix_status_hid_result(bool success)
{ portENTER_CRITICAL(&lock); matrix_model_hid_result(&state, success, now_ms()); portEXIT_CRITICAL(&lock); }
void matrix_status_host_task_started(void)
{ portENTER_CRITICAL(&lock); matrix_model_host_task(&state, now_ms()); portEXIT_CRITICAL(&lock); }
void matrix_status_note_lcd_command(bool valid)
{ portENTER_CRITICAL(&lock); matrix_model_i2c(&state, valid, now_ms()); portEXIT_CRITICAL(&lock); }
void matrix_status_set_ble_gamepad(matrix_link_t link)
{ if (link > MATRIX_CONNECTED) return; portENTER_CRITICAL(&lock); state.ble_pad = link;
  if (link != MATRIX_CONNECTED) { state.ble_battery = 255; } portEXIT_CRITICAL(&lock); }
void matrix_status_set_ble_gimbal(matrix_link_t link)
{ if (link > MATRIX_CONNECTED) return; portENTER_CRITICAL(&lock); state.gimbal = link;
  if (link != MATRIX_CONNECTED) { state.gimbal_battery = 255; } portEXIT_CRITICAL(&lock); }
void matrix_status_set_ble_gamepad_battery(uint8_t percent)
{ portENTER_CRITICAL(&lock); state.ble_battery = state.ble_pad == MATRIX_CONNECTED && percent <= 100 ? percent : 255; portEXIT_CRITICAL(&lock); }
void matrix_status_set_ble_gimbal_battery(uint8_t percent)
{ portENTER_CRITICAL(&lock); state.gimbal_battery = state.gimbal == MATRIX_CONNECTED && percent <= 100 ? percent : 255; portEXIT_CRITICAL(&lock); }
uint8_t matrix_status_faults(void)
{ portENTER_CRITICAL(&lock); uint8_t value = state.faults; portEXIT_CRITICAL(&lock); return value; }
void matrix_status_set_gimbal_fault(bool active)
{ portENTER_CRITICAL(&lock); if (active) state.faults|=MATRIX_GIMBAL; else state.faults&=~MATRIX_GIMBAL; portEXIT_CRITICAL(&lock); }

void matrix_status_get_state(matrix_model_t *out)
{ portENTER_CRITICAL(&lock); *out = state; portEXIT_CRITICAL(&lock); }

void matrix_status_debug_get(bool *calibration, uint8_t *forced)
{
    *calibration=false; *forced=0;
#if CONFIG_REMOTE_DBG_SIM
    portENTER_CRITICAL(&lock); *calibration=debug.calibration; *forced=debug.forced; portEXIT_CRITICAL(&lock);
#endif
}
bool matrix_status_debug_command(int argc, char **argv)
{
#if CONFIG_REMOTE_DBG_SIM
    if (strcmp(argv[0],"led")) return false;
    bool valid=false; uint8_t mask=0;
    if (argc==2 && (!strcmp(argv[1],"test") || !strcmp(argv[1],"off"))) {
        portENTER_CRITICAL(&lock);
        if (!strcmp(argv[1],"test")) { debug.calibration=!debug.calibration; debug.started_ms=now_ms(); }
        else debug=(matrix_debug_t){0};
        portEXIT_CRITICAL(&lock); valid=true;
    } else if (argc==4 && !strcmp(argv[1],"fault") &&
               (!strcmp(argv[3],"on") || !strcmp(argv[3],"off"))) {
        mask=!strcmp(argv[2],"bt")?MATRIX_BLUETOOTH:!strcmp(argv[2],"i2c")?MATRIX_PROTOCOL:
             !strcmp(argv[2],"overflow")?MATRIX_OVERFLOW:0;
        if (mask) {
            portENTER_CRITICAL(&lock);
            if (!strcmp(argv[3],"on")) debug.forced |= mask; else debug.forced &= ~mask;
            portEXIT_CRITICAL(&lock); valid=true;
        }
    }
    if (!valid) { debug_printf("[dbg] ERR SIM led: test; off; fault bt|i2c|overflow on|off\n"); return true; }
    bool calibration; uint8_t forced; matrix_status_debug_get(&calibration,&forced);
    debug_printf("[dbg] OK SIM led calibration=%d forced=0x%02x RAM only\n",calibration,forced);
    return true;
#else
    (void)argc; (void)argv; return false;
#endif
}

static esp_err_t rmt_start(void)
{
    const rmt_tx_channel_config_t config = {.gpio_num = 27, .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000, .mem_block_symbols = 64, .trans_queue_depth = 1};
    ESP_RETURN_ON_ERROR(rmt_new_tx_channel(&config, &channel), TAG, "create channel");
    const rmt_bytes_encoder_config_t bytes = {
        .bit0 = {.level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9},
        .bit1 = {.level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3}, .flags.msb_first = 1};
    ESP_RETURN_ON_ERROR(rmt_new_bytes_encoder(&bytes, &encoder), TAG, "create encoder");
    return rmt_enable(channel);
}
static esp_err_t rmt_stop(void)
{
    if (channel) {
        esp_err_t err = rmt_disable(channel);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
        ESP_RETURN_ON_ERROR(rmt_del_channel(channel), TAG, "delete channel");
        channel = NULL;
    }
    if (encoder) {
        ESP_RETURN_ON_ERROR(rmt_del_encoder(encoder), TAG, "delete encoder");
        encoder = NULL;
    }
    return ESP_OK;
}
static esp_err_t show(const uint8_t colors[25])
{
    matrix_frame_grb(colors, grb);
    const rmt_transmit_config_t config = {.loop_count = 0};
    ESP_RETURN_ON_ERROR(rmt_transmit(channel, encoder, grb, sizeof(grb), &config), TAG, "transmit");
    ESP_RETURN_ON_ERROR(rmt_tx_wait_all_done(channel, pdMS_TO_TICKS(100)), TAG, "wait");
    esp_rom_delay_us(80);
    return ESP_OK;
}
static void render_task(void *context)
{
    (void)context;
    matrix_frame_cache_t cache = {0};
    bool recovering = false, stopped = false, logged_hid_fault = false, recovery_attempt = false;
    uint32_t retry_ms = 0, last_error_log = 0;
    unsigned recovery_failures = 0;
    TickType_t cycle = xTaskGetTickCount();
    for (;;) {
        uint32_t now = now_ms(), dropped;
        uint8_t classic;
        ds4_host_status(&classic, &dropped);
        ds4_state_t pad;
        ds4_host_get_classic(&pad);
        portENTER_CRITICAL(&lock);
        state.classic = (matrix_link_t)classic;
        state.classic_battery = classic == MATRIX_CONNECTED && pad.connected && pad.battery <= 10 ? pad.battery * 10 : 255;
        matrix_model_dropped(&state, dropped, now);
        bool was_waiting = state.hid_waiting;
        matrix_model_tick(&state, now);
        bool hid_timed_out = was_waiting && !state.hid_waiting && !state.hid_result;
        matrix_model_t snapshot = state;
#if CONFIG_REMOTE_DBG_SIM
        matrix_debug_t debug_snapshot=debug;
#endif
        portEXIT_CRITICAL(&lock);
        if (hid_timed_out && !logged_hid_fault) {
            ESP_LOGE(TAG, "HID Host asynchronous initialization timed out (3 s)"); logged_hid_fault = true;
        }
        if (stopped) {
            if ((uint32_t)(now - last_error_log) >= 5000) {
                ESP_LOGE(TAG, "LED recovery stopped after 5 failures; Bluetooth/I2C continue"); last_error_log = now;
            }
        } else if (recovering) {
            if ((int32_t)(now - retry_ms) >= 0) {
                esp_err_t err = rmt_stop();
                if (err == ESP_OK) err = rmt_start();
                if (err == ESP_OK) { recovering = false; cache.sent = false; recovery_attempt = true; }
                else {
                    ++recovery_failures; retry_ms = now + 1000;
                    ESP_LOGE(TAG, "LED recovery %u/5 failed: %s", recovery_failures, esp_err_to_name(err));
                    if (recovery_failures >= 5) { stopped = true; last_error_log = now; }
                }
            }
        } else {
            uint8_t colors[25];
#if CONFIG_REMOTE_DBG_SIM
            matrix_debug_frame(&debug_snapshot,&snapshot,now,colors);
#else
            matrix_model_frame(&snapshot, now, colors);
#endif
            if (matrix_frame_due(&cache, colors, now)) {
                esp_err_t err = show(colors);
                if (err == ESP_OK) {
                    matrix_frame_sent(&cache, colors, now); recovery_failures = 0; recovery_attempt = false;
                } else {
                    /* Do not overwrite grb again until the old channel has
                     * been disabled/deleted, even after a wait timeout. */
                    recovering = true; retry_ms = now + 1000;
                    if (recovery_attempt && ++recovery_failures >= 5) { stopped = true; last_error_log = now; }
                    recovery_attempt = false;
                    ESP_LOGE(TAG, "LED frame failed: %s; retry in 1 s", esp_err_to_name(err));
                }
            }
        }
        vTaskDelayUntil(&cycle, pdMS_TO_TICKS(125));
    }
}
esp_err_t matrix_status_init(void)
{
    matrix_model_init(&state, now_ms());
    ESP_RETURN_ON_ERROR(rmt_start(), TAG, "initialize LED");
    const uint8_t off[25] = {0};
    ESP_RETURN_ON_ERROR(show(off), TAG, "clear previous frame");
    return xTaskCreatePinnedToCore(render_task, "matrix_render", 3072, NULL, 2, NULL, 1) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
