#include "wifi_ap.h"
#include <string.h>
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "board_7b.h"

static const char *TAG = "wifi_ap";
static esp_netif_t *ap_netif;

void wifi_ap_log_clients(void)
{
    wifi_sta_list_t clients = {0};
    esp_err_t err = esp_wifi_ap_get_sta_list(&clients);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Client query failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Connected clients: %d", clients.num);
    if (!clients.num) {
        board_7b_set_wifi_rssi(-127);
        return;
    }
    board_7b_set_wifi_rssi(clients.sta[0].rssi);
    esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM] = {0};
    for (int i = 0; i < clients.num; ++i) {
        memcpy(pairs[i].mac, clients.sta[i].mac, 6);
    }
    err = esp_netif_dhcps_get_clients_by_mac(ap_netif, clients.num, pairs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "DHCP query failed: %s", esp_err_to_name(err));
    }
    for (int i = 0; i < clients.num; ++i) {
        ESP_LOGI(TAG, "Client MAC=" MACSTR " IP=" IPSTR " RSSI=%d dBm",
                 MAC2STR(clients.sta[i].mac), IP2STR(&pairs[i].ip), clients.sta[i].rssi);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (id == WIFI_EVENT_AP_START) {
        esp_netif_ip_info_t ip;
        ESP_ERROR_CHECK(esp_netif_get_ip_info(ap_netif, &ip));
        ESP_LOGI(TAG, "AP READY: SSID=%s channel=6 WPA2-PSK IP=" IPSTR,
                 AP_SSID, IP2STR(&ip.ip));
    } else if (id == WIFI_EVENT_AP_STACONNECTED) {
        const wifi_event_ap_staconnected_t *event = data;
        ESP_LOGI(TAG, "Client connected: AID=%d", event->aid);
    } else if (id == WIFI_EVENT_AP_STADISCONNECTED) {
        const wifi_event_ap_stadisconnected_t *event = data;
        ESP_LOGI(TAG, "Client disconnected: AID=%d reason=%d", event->aid, event->reason);
    }
}

void wifi_ap_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // Default AP interface provides 192.168.4.1/24 and a DHCP server.
    ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(ap_netif ? ESP_OK : ESP_ERR_NO_MEM);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    wifi_config_t config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = sizeof(AP_SSID) - 1,
            .password = AP_PASSWORD,
            .channel = 6,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 4,
            .pmf_cfg.required = false,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
}
