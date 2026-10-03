#include "wifi_ap.h"
#include "wifi_apply.h"
#include "factory_reset.h"
#include "ui_preferences.h"
#include "debug_console.h"
#include "camera_pair.h"
#include "camera_identity.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include "nvs.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdio.h>
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "board_7b.h"


static const char *TAG = "wifi_ap";
static esp_netif_t *ap_netif;
static portMUX_TYPE camera_mux = portMUX_INITIALIZER_UNLOCKED;
static uint8_t selected_mac[6];
static bool camera_selected;
void wifi_ap_select_camera(const uint8_t mac[6])
{
    portENTER_CRITICAL(&camera_mux);
    camera_selected = mac != NULL;
    if (mac) memcpy(selected_mac, mac, 6);
    portEXIT_CRITICAL(&camera_mux);
}

static void update_clients(bool log)
{
    wifi_sta_list_t clients = {0};
    esp_err_t err = esp_wifi_ap_get_sta_list(&clients);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Client query failed: %s", esp_err_to_name(err));
        return;
    }
    if (log) ESP_LOGI(TAG, "Connected clients: %d", clients.num);
    if (!clients.num) {
        board_7b_set_wifi_rssi(-127);
        return;
    }
    uint8_t mac[6];
    portENTER_CRITICAL(&camera_mux);
    bool selected = camera_selected;
    memcpy(mac, selected_mac, 6);
    portEXIT_CRITICAL(&camera_mux);
    int rssi = -127;
    for (int i = 0; i < clients.num; ++i)
        if (selected && !memcmp(mac, clients.sta[i].mac, 6)) rssi = clients.sta[i].rssi;
    board_7b_set_wifi_rssi(rssi);
    esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM] = {0};
    for (int i = 0; i < clients.num; ++i) {
        memcpy(pairs[i].mac, clients.sta[i].mac, 6);
    }
    err = esp_netif_dhcps_get_clients_by_mac(ap_netif, clients.num, pairs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHCP query failed: %s", esp_err_to_name(err));
    }
    for (int i = 0; i < clients.num; ++i) {
        if (log) ESP_LOGI(TAG, "Client MAC=" MACSTR " IP=" IPSTR " RSSI=%d dBm",
                 MAC2STR(clients.sta[i].mac), IP2STR(&pairs[i].ip), clients.sta[i].rssi);
    }
}

void wifi_ap_log_clients(void) { update_clients(true); }

bool wifi_ap_get_clients(wifi_ap_client_t *out, size_t capacity, size_t *count)
{
    if (!out || !count || !ap_netif) return false;
    *count = 0;
    wifi_sta_list_t clients = {0};
    if (esp_wifi_ap_get_sta_list(&clients) != ESP_OK || clients.num > ESP_WIFI_MAX_CONN_NUM) return false;
    esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM] = {0};
    for (int i = 0; i < clients.num; ++i) memcpy(pairs[i].mac, clients.sta[i].mac, 6);
    if (!clients.num) return true;
    if (esp_netif_dhcps_get_clients_by_mac(ap_netif, clients.num, pairs) != ESP_OK) return false;
    for (int i = 0; i < clients.num; ++i) {
        if (!pairs[i].ip.addr) continue;
        if (*count >= capacity) return false;
        wifi_ap_client_t *client = &out[(*count)++];
        memcpy(client->mac, clients.sta[i].mac, 6);
        snprintf(client->ip, sizeof(client->ip), IPSTR, IP2STR(&pairs[i].ip));
        client->rssi = clients.sta[i].rssi;
    }
    return true;
}

static portMUX_TYPE config_mux = portMUX_INITIALIZER_UNLOCKED;
static app_wifi_config_t current_config;
static bool config_loaded;
static unsigned channel_limit = 13;
typedef struct { app_wifi_config_t config; uint32_t token; bool reset_all, staged; } apply_request_t;
typedef struct { uint32_t token,ready_ms; esp_err_t result; bool used, complete, staged,released,cancelled; } apply_result_t;
static atomic_uint network_generation;
uint32_t wifi_ap_network_generation(void) { return atomic_load(&network_generation); }
static apply_result_t results[8];
static bool reset_queued;
static QueueHandle_t requests;
static SemaphoreHandle_t enqueue_lock;
static esp_err_t store_error, driver_error;
void wifi_ap_get_config(app_wifi_config_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&config_mux);
    if (config_loaded) *out = current_config;
    else wifi_config_make_default(out);
    portEXIT_CRITICAL(&config_mux);
}
unsigned wifi_ap_max_channel(void)
{
    portENTER_CRITICAL(&config_mux); unsigned limit = channel_limit; portEXIT_CRITICAL(&config_mux);
    return limit;
}
static bool save_config(void *unused, const app_wifi_config_t *config)
{
    (void)unused;
    uint8_t bytes[WIFI_CONFIG_RECORD_SIZE];
    if (!wifi_config_encode(config, bytes)) { store_error = ESP_ERR_INVALID_ARG; return false; }
    nvs_handle_t nvs;
    store_error = nvs_open("wifi_ap", NVS_READWRITE, &nvs);
    if (store_error != ESP_OK) return false;
    app_wifi_config_t defaults; wifi_config_make_default(&defaults);
    if (wifi_config_equal(config, &defaults)) {
        store_error = nvs_erase_key(nvs, "cfg");
        if (store_error == ESP_ERR_NVS_NOT_FOUND) store_error = ESP_OK;
    } else store_error = nvs_set_blob(nvs, "cfg", bytes, sizeof(bytes));
    if (store_error == ESP_OK) store_error = nvs_commit(nvs);
    nvs_close(nvs); return store_error == ESP_OK;
}
void wifi_ap_load_config(void)
{
    app_wifi_config_t config; wifi_config_make_default(&config);
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("wifi_ap", NVS_READONLY, &nvs);
    bool corrupt = false;
    if (err == ESP_OK) {
        uint8_t bytes[WIFI_CONFIG_RECORD_SIZE]; size_t size = sizeof(bytes);
        err = nvs_get_blob(nvs, "cfg", bytes, &size);
        if (err == ESP_OK) corrupt = !wifi_config_decode(bytes, size, &config);
        else corrupt = err != ESP_ERR_NVS_NOT_FOUND;
        nvs_close(nvs);
    }
    if (corrupt) {
        wifi_config_make_default(&config);
        ESP_LOGW(TAG, "Invalid Wi-Fi record (%s); restoring defaults", esp_err_to_name(err));
        if (!save_config(NULL, &config)) ESP_LOGE(TAG, "Cannot remove invalid Wi-Fi record: %s", esp_err_to_name(store_error));
    } else if (err == ESP_ERR_NVS_NOT_FOUND) ESP_LOGI(TAG, "Wi-Fi defaults: no saved record");
    else if (err != ESP_OK) ESP_LOGW(TAG, "Wi-Fi load failed: %s; using defaults", esp_err_to_name(err));
    portENTER_CRITICAL(&config_mux);
    current_config = config; config_loaded = true;
    portEXIT_CRITICAL(&config_mux);
}
static void publish_wifi_info(bool online)
{
    app_wifi_config_t config; wifi_ap_get_config(&config);
    char ip_text[16] = {0}; esp_netif_ip_info_t ip;
    if (online && ap_netif && esp_netif_is_netif_up(ap_netif) && esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK)
        snprintf(ip_text, sizeof(ip_text), IPSTR, IP2STR(&ip.ip));
    board_7b_set_wifi_info(config.ssid, config.password, config.show_password, ip_text,wifi_config_uses_default_password(&config));
}
static esp_err_t configure_driver(const app_wifi_config_t *settings)
{
    wifi_config_t config = {0};
    size_t ssid_size = strlen(settings->ssid), password_size = strlen(settings->password);
    memcpy(config.ap.ssid, settings->ssid, ssid_size);
    memcpy(config.ap.password, settings->password, password_size);
    config.ap.ssid_len = ssid_size; config.ap.channel = settings->channel;
    config.ap.authmode = WIFI_AUTH_WPA2_PSK; config.ap.max_connection = WIFI_AP_CLIENT_CAPACITY;
    config.ap.pmf_cfg.required = false;
    return esp_wifi_set_config(WIFI_IF_AP, &config);
}
static bool restart_driver(void *unused, const app_wifi_config_t *config)
{
    (void)unused;
    atomic_fetch_add(&network_generation,1);
    publish_wifi_info(false); board_7b_set_wifi_rssi(-127);
    driver_error = esp_wifi_stop();
    if (driver_error == ESP_OK) driver_error = configure_driver(config);
    if (driver_error == ESP_OK) driver_error = esp_wifi_start();
    return driver_error == ESP_OK;
}
static esp_err_t enqueue_config(const app_wifi_config_t *config, bool reset_all,bool staged, uint32_t *token)
{
    if (!config || !token || !requests || !enqueue_lock) return ESP_ERR_INVALID_STATE;
    if (wifi_config_check(config, wifi_ap_max_channel()) != WIFI_CFG_OK) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(enqueue_lock, 0) != pdTRUE) return ESP_ERR_INVALID_STATE;
    apply_request_t request = {.config = *config, .reset_all = reset_all,.staged=staged};
    portENTER_CRITICAL(&config_mux);
    if (reset_queued) { portEXIT_CRITICAL(&config_mux); xSemaphoreGive(enqueue_lock); return ESP_ERR_INVALID_STATE; }
    unsigned slot = 8;
    for (unsigned i = 0; i < 8; ++i) if (!results[i].used) { slot = i; break; }
    if (slot == 8) {
        for (unsigned i = 0; i < 8; ++i) {
            if (results[i].complete && (slot == 8 || (int32_t)(results[i].token - results[slot].token) < 0)) slot = i;
        }
    }
    if (slot == 8) { portEXIT_CRITICAL(&config_mux); xSemaphoreGive(enqueue_lock); return ESP_ERR_INVALID_STATE; }
    request.token = debug_async_token();
    results[slot] = (apply_result_t){.used = true, .token = request.token,.staged=staged};
    if (reset_all) reset_queued = true;
    portEXIT_CRITICAL(&config_mux);
    if (xQueueSend(requests, &request, 0) != pdTRUE) {
        portENTER_CRITICAL(&config_mux); results[slot].used = false;
        if (reset_all) reset_queued = false;
        portEXIT_CRITICAL(&config_mux);
        xSemaphoreGive(enqueue_lock);
        return ESP_ERR_INVALID_STATE;
    }
    *token = request.token; xSemaphoreGive(enqueue_lock); return ESP_OK;
}
esp_err_t wifi_ap_request_apply(const app_wifi_config_t *config, uint32_t *token)
{ return enqueue_config(config, false,false, token); }
esp_err_t wifi_ap_prepare_apply(const app_wifi_config_t *config,uint32_t *token)
{ return enqueue_config(config,false,true,token); }
esp_err_t wifi_ap_commit_apply(uint32_t token,unsigned delay_ms)
{
    if (!token || delay_ms>10000) return ESP_ERR_INVALID_ARG;
    esp_err_t error=ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&config_mux);
    for (unsigned i=0;i<8;++i) if (results[i].used && results[i].token==token &&
        results[i].staged && !results[i].complete && !results[i].released && !results[i].cancelled) {
        results[i].ready_ms=(uint32_t)(esp_timer_get_time()/1000)+delay_ms;
        results[i].released=true;error=ESP_OK;break;
    }
    portEXIT_CRITICAL(&config_mux);return error;
}
void wifi_ap_cancel_apply(uint32_t token)
{
    portENTER_CRITICAL(&config_mux);
    for (unsigned i=0;i<8;++i) if (results[i].used && results[i].token==token &&
        results[i].staged && !results[i].released) results[i].cancelled=true;
    portEXIT_CRITICAL(&config_mux);
}
static esp_err_t wait_staged(uint32_t token)
{
    uint32_t started=(uint32_t)(esp_timer_get_time()/1000);
    for (;;) {
        bool found=false,cancelled=false,released=false;uint32_t ready=0;
        portENTER_CRITICAL(&config_mux);
        for (unsigned i=0;i<8;++i) if (results[i].used && results[i].token==token) {
            found=true;cancelled=results[i].cancelled;released=results[i].released;ready=results[i].ready_ms;break;
        }
        portEXIT_CRITICAL(&config_mux);
        uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
        if (!found || cancelled) return ESP_ERR_INVALID_STATE;
        if (released && (int32_t)(now-ready)>=0) return ESP_OK;
        if ((uint32_t)(now-started)>=25000) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
esp_err_t wifi_ap_request_reset(bool all, uint32_t *token)
{
    app_wifi_config_t defaults; wifi_config_make_default(&defaults);
    return enqueue_config(&defaults, all,false, token);
}
static bool acquire_camera(void *unused)
{ (void)unused; return camera_maintenance_acquire(5000); }
static void release_camera(void *unused)
{ (void)unused; camera_maintenance_release(); }
static bool forget_camera(void *unused)
{ (void)unused; return camera_identity_forget(); }
static bool reset_ui(void *unused)
{ (void)unused;return ui_preferences_reset()==ESP_OK; }
esp_err_t wifi_ap_request_result(uint32_t token, esp_err_t *result)
{
    if (!token || !result) return ESP_ERR_INVALID_ARG;
    esp_err_t state = ESP_ERR_NOT_FOUND;
    portENTER_CRITICAL(&config_mux);
    for (unsigned i = 0; i < 8; ++i) if (results[i].used && results[i].token == token) {
        state = results[i].complete ? ESP_OK : ESP_ERR_NOT_FINISHED;
        if (results[i].complete) *result = results[i].result;
        break;
    }
    portEXIT_CRITICAL(&config_mux); return state;
}
static void config_task(void *unused)
{
    (void)unused; TickType_t last_clients = xTaskGetTickCount(), last_log = last_clients;
    for (;;) {
        apply_request_t request;
        if (xQueueReceive(requests, &request, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (request.staged) {
                esp_err_t staged=wait_staged(request.token);
                if (staged!=ESP_OK) {
                    portENTER_CRITICAL(&config_mux);
                    for (unsigned i=0;i<8;++i) if (results[i].used && results[i].token==request.token) {
                        results[i].result=staged;results[i].complete=true;break;
                    }
                    portEXIT_CRITICAL(&config_mux);continue;
                }
            }
            app_wifi_config_t current; wifi_ap_get_config(&current);
            if (request.reset_all) {
                const factory_reset_ops_t ops = {acquire_camera, release_camera, save_config, forget_camera,reset_ui};
                factory_reset_result_t reset = factory_reset_all(&current, &ops, NULL);
                esp_err_t error = reset == FACTORY_RESET_OK ? ESP_OK : reset == FACTORY_RESET_BUSY ? ESP_ERR_TIMEOUT : ESP_FAIL;
                portENTER_CRITICAL(&config_mux);
                if (reset == FACTORY_RESET_OK) current_config = request.config;
                else reset_queued = false;
                for (unsigned i = 0; i < 8; ++i) if (results[i].used && results[i].token == request.token) {
                    results[i].result = error; results[i].complete = true; break;
                }
                portEXIT_CRITICAL(&config_mux);
                ESP_LOGI(TAG, "Factory all token=%lu result=%d; wifi_ap/cfg, sony_remote and ui_prefs/info targeted",
                    (unsigned long)request.token, reset);
                if (reset == FACTORY_RESET_OK) {
                    wifi_ap_select_camera(NULL); publish_wifi_info(true);
                    /* Allow console / screen to observe completion before reboot. */
                    vTaskDelay(pdMS_TO_TICKS(500)); esp_restart();
                } else ESP_LOGE(TAG, "Factory all failed; no reboot. Persisted keys may have changed; result=%d", reset);
                continue;
            }
            bool network_change = !wifi_config_network_equal(&current, &request.config);
            wifi_apply_result_t applied = wifi_apply_config(&current, &request.config, wifi_ap_max_channel(), save_config, restart_driver, NULL);
            esp_err_t err = applied == WIFI_APPLY_OK ? ESP_OK : applied == WIFI_APPLY_INVALID ? ESP_ERR_INVALID_ARG :
                            applied == WIFI_APPLY_SAVE_FAILED ? store_error : ESP_FAIL;
            portENTER_CRITICAL(&config_mux);
            if (applied == WIFI_APPLY_OK) current_config = current;
            for (unsigned i = 0; i < 8; ++i) if (results[i].used && results[i].token == request.token) {
                results[i].result = err; results[i].complete = true; break;
            }
            portEXIT_CRITICAL(&config_mux);
            publish_wifi_info(applied != WIFI_APPLY_ROLLBACK_FAILED);
            ESP_LOGI(TAG, "Apply token=%lu result=%d SSID=%s channel=%u password_len=%u%s",
                (unsigned long)request.token, applied, current.ssid, current.channel, (unsigned)strlen(current.password),
                applied == WIFI_APPLY_OK && network_change ? "; reconnect camera to new Wi-Fi" : "");
            if (applied == WIFI_APPLY_ROLLBACK_FAILED) ESP_LOGE(TAG, "Wi-Fi rollback incomplete; save=%s driver=%s", esp_err_to_name(store_error), esp_err_to_name(driver_error));
        }
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - last_clients) >= pdMS_TO_TICKS(2000)) {
            bool log = (TickType_t)(now - last_log) >= pdMS_TO_TICKS(10000);
            update_clients(log); last_clients = now; if (log) last_log = now;
        }
        board_7b_refresh_wifi_info();
    }
}
static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == WIFI_EVENT_AP_START) {
        publish_wifi_info(true);
        wifi_config_t config; esp_netif_ip_info_t ip;
        if (esp_wifi_get_config(WIFI_IF_AP, &config) == ESP_OK && esp_netif_get_ip_info(ap_netif, &ip) == ESP_OK)
            ESP_LOGI(TAG, "AP READY: SSID=%.*s channel=%u WPA2-PSK password_len=%u IP=" IPSTR,
                config.ap.ssid_len, config.ap.ssid, config.ap.channel, (unsigned)strlen((char *)config.ap.password), IP2STR(&ip.ip));
    } else if (id == WIFI_EVENT_AP_STOP) publish_wifi_info(false);
    else if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = data; ESP_LOGI(TAG, "Client connected: AID=%d", event->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = data; ESP_LOGI(TAG, "Client disconnected: AID=%d reason=%d", event->aid, event->reason);
    }
}
void wifi_ap_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init()); ESP_ERROR_CHECK(esp_event_loop_create_default());
    ap_netif = esp_netif_create_default_wifi_ap(); ESP_ERROR_CHECK(ap_netif ? ESP_OK : ESP_ERR_NO_MEM);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); init.nvs_enable = false;
    ESP_ERROR_CHECK(esp_wifi_init(&init)); ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_country_code(CONFIG_APP_WIFI_COUNTRY, true));
    wifi_country_t country; ESP_ERROR_CHECK(esp_wifi_get_country(&country));
    unsigned limit = country.schan + country.nchan - 1; if (limit > 13) limit = 13;
    portENTER_CRITICAL(&config_mux); channel_limit = limit; portEXIT_CRITICAL(&config_mux);
    app_wifi_config_t config; wifi_ap_get_config(&config);
    if (wifi_config_check(&config, limit) != WIFI_CFG_OK) {
        ESP_LOGW(TAG, "Saved Wi-Fi config outside country range; using defaults");
        wifi_config_make_default(&config);
        if (!save_config(NULL, &config)) ESP_LOGE(TAG, "Cannot remove incompatible Wi-Fi record: %s", esp_err_to_name(store_error));
        portENTER_CRITICAL(&config_mux); current_config = config; portEXIT_CRITICAL(&config_mux);
    }
    requests = xQueueCreate(2, sizeof(apply_request_t)); ESP_ERROR_CHECK(requests ? ESP_OK : ESP_ERR_NO_MEM);
    enqueue_lock = xSemaphoreCreateMutex(); ESP_ERROR_CHECK(enqueue_lock ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP)); ESP_ERROR_CHECK(configure_driver(&config)); ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(xTaskCreate(config_task, "wifi_config", 4096, NULL, 2, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
