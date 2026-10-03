#include "wifi_menu_ui.h"
#include "wifi_menu.h"
#include "wifi_ap.h"
#include "board_7b.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

static QueueHandle_t inputs;
static atomic_bool owns_page;
static atomic_bool open_pending;
static atomic_bool cancel_page;
bool wifi_menu_ui_active(void) { return atomic_load(&owns_page); }
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
bool wifi_menu_ui_action(pad_action_t action)
{
    if (!inputs) return false;
    if (action.type == PAD_ACTION_RELEASE_ALL) {
        atomic_store(&cancel_page, true); return true;
    }
    if (!wifi_menu_ui_active() && action.type == PAD_ACTION_MENU_MOVE) {
        board_7b_menu_move(action.value); return true;
    }
    bool opening = !wifi_menu_ui_active() && board_7b_menu_selected() == 7 &&
        (action.type == PAD_ACTION_MENU_CONFIRM || (action.type == PAD_ACTION_MENU_STEP && action.value > 0));
    if (opening) { atomic_store(&open_pending, true); atomic_store(&owns_page, true); }
    if (xQueueSend(inputs, &action, 0) == pdTRUE) return true;
    if (opening) { atomic_store(&open_pending, false); atomic_store(&owns_page, false); }
    return false;
}
static void publish(const wifi_menu_t *m, const char *status)
{
    board_wifi_menu_view_t v = {.active = m->active, .selected = m->row};
    snprintf(v.lines[0], sizeof(v.lines[0]), "SSID %s%s", m->editing ? m->edit : m->draft.ssid,
        strcmp(m->draft.ssid, m->actual.ssid) ? " *" : "");
    snprintf(v.lines[1], sizeof(v.lines[1]), "PASS %s%s%s", m->draft.show_password ? m->draft.password : "********",
        strcmp(m->draft.password, m->actual.password) ? " *" : "",wifi_config_uses_default_password(&m->draft)?" DEFAULT":"");
    snprintf(v.lines[2], sizeof(v.lines[2]), "NEW PASSWORD");
    snprintf(v.lines[3], sizeof(v.lines[3]), "CHANNEL < %u >%s", m->draft.channel,
        m->draft.channel != m->actual.channel ? " *" : "");
    snprintf(v.lines[4], sizeof(v.lines[4]), "SHOW PASS %s", m->draft.show_password ? "ON" : "OFF");
    snprintf(v.lines[5], sizeof(v.lines[5]), m->pending ? "APPLYING..." : wifi_config_equal(&m->draft, &m->actual) ? "APPLY (no changes)" : "APPLY *");
    snprintf(v.lines[6], sizeof(v.lines[6]), m->confirm_reset && m->row == WIFI_ROW_RESET ? "PRESS AGAIN (3s)" : "RESET WI-FI");
    snprintf(v.lines[7], sizeof(v.lines[7]), m->confirm_reset && m->row == WIFI_ROW_RESET_ALL ? "PRESS AGAIN (3s)" : "RESET ALL (reboot)");
    snprintf(v.lines[8], sizeof(v.lines[8]), "BACK");
    if (m->editing) snprintf(v.footer, sizeof(v.footer), "%sChar %u [%c] U/D change L/R move A done B cancel",
        !*m->edit ? "SSID empty! " : "", m->cursor + 1, m->edit[m->cursor] ? m->edit[m->cursor] : '~');
    else snprintf(v.footer, sizeof(v.footer), "%s", status);
    board_7b_set_wifi_menu(&v);
    if (m->active) atomic_store(&owns_page, true);
    else if (!atomic_load(&open_pending)) atomic_store(&owns_page, false);
}
static void task(void *unused)
{
    (void)unused;
    wifi_menu_t menu = {0};
    uint32_t token = 0, connection_generation = board_7b_connection_generation();
    bool display_only = false, reset_all = false;
    char status[96] = "A confirm / B back";
    for (;;) {
        pad_action_t action;
        bool changed = false;
        if (atomic_exchange(&cancel_page, false)) {
            xQueueReset(inputs);
            atomic_store(&open_pending, false);
            menu.active = menu.editing = menu.confirm_reset = false;
            changed = true;
        }
        if (xQueueReceive(inputs, &action, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (action.type == PAD_ACTION_MENU_CONFIRM || action.type == PAD_ACTION_MENU_STEP) atomic_store(&open_pending, false);
            if (action.type == PAD_ACTION_RELEASE_ALL || !board_7b_settings_mode()) menu.active = false;
            else if (!menu.active) {
                if ((action.type == PAD_ACTION_MENU_CONFIRM ||
                     (action.type == PAD_ACTION_MENU_STEP && action.value > 0)) && board_7b_menu_selected() == 7) {
                    app_wifi_config_t config; wifi_ap_get_config(&config);
                    wifi_menu_open(&menu, &config, wifi_ap_max_channel());
                    menu.pending = token != 0;
                    snprintf(status, sizeof(status), "A confirm / B back");
                } else if (action.type == PAD_ACTION_MENU_MOVE) board_7b_menu_move(action.value);
                /* Camera parameter steps are routed at the input edge, not replayed here. */
            } else {
                wifi_menu_input_t input = action.type == PAD_ACTION_MENU_MOVE ? WIFI_MENU_MOVE :
                    action.type == PAD_ACTION_MENU_STEP ? WIFI_MENU_STEP :
                    action.type == PAD_ACTION_MENU_CONFIRM ? WIFI_MENU_CONFIRM : WIFI_MENU_BACK;
                wifi_menu_effect_t effect = wifi_menu_input(&menu, input, action.value, now_ms());
                if (effect == WIFI_MENU_RANDOM) wifi_config_make_password(menu.draft.password, esp_fill_random);
                else if (effect == WIFI_MENU_RESET_ALL) {
                    esp_err_t err = wifi_ap_request_reset(true, &token);
                    menu.pending = err == ESP_OK; reset_all = err == ESP_OK; display_only = false;
                    snprintf(status, sizeof(status), "%s", err == ESP_OK ? "Stopping camera, resetting..." : esp_err_to_name(err));
                    if (err != ESP_OK) token = 0;
                }
                else if (effect == WIFI_MENU_APPLY || effect == WIFI_MENU_DISPLAY) {
                    app_wifi_config_t current; wifi_ap_get_config(&current);
                    if (!wifi_config_equal(&current, &menu.actual)) {
                        wifi_menu_complete(&menu, &current, false, false);
                        snprintf(status, sizeof(status), "Config changed; review draft");
                    } else {
                        display_only = effect == WIFI_MENU_DISPLAY;
                        reset_all = false;
                        app_wifi_config_t next = display_only ? current : menu.draft;
                        if (display_only) next.show_password = menu.draft.show_password;
                        esp_err_t err = wifi_ap_request_apply(&next, &token);
                        menu.pending = err == ESP_OK;
                        snprintf(status, sizeof(status), "%s", err == ESP_OK ? "Saving..." : esp_err_to_name(err));
                        if (err != ESP_OK) { token = 0; menu.draft.show_password = menu.actual.show_password; }
                    }
                }
            }
            changed = true;
        }
        if (token) {
            esp_err_t result;
            esp_err_t state = wifi_ap_request_result(token, &result);
            if (state != ESP_ERR_NOT_FINISHED) {
                app_wifi_config_t current; wifi_ap_get_config(&current);
                if (state != ESP_OK) result = state;
                wifi_menu_complete(&menu, &current, result == ESP_OK, display_only);
                snprintf(status, sizeof(status), "%s", result == ESP_OK ?
                    (reset_all ? "Reset complete; rebooting" : display_only ? "Password display saved" : "Reconnect camera to Wi-Fi") : esp_err_to_name(result));
                token = 0; changed = true;
            }
        }
        bool confirmed = menu.confirm_reset;
        wifi_menu_tick(&menu, now_ms());
        if (confirmed != menu.confirm_reset) changed = true;
        uint32_t connection = board_7b_connection_generation();
        if (connection != connection_generation || !board_7b_settings_mode()) {
            connection_generation = connection;
            if (menu.active) { menu.active = menu.editing = menu.confirm_reset = false; changed = true; }
        }
        if (changed) publish(&menu, status);
        board_7b_refresh_wifi_info();
    }
}
void wifi_menu_ui_start(void)
{
    inputs = xQueueCreate(16, sizeof(pad_action_t));
    ESP_ERROR_CHECK(inputs ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(task, "wifi_menu", 4096, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
