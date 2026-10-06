#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
#define ESP_WIFI_MAX_CONN_NUM 4
#define WIFI_IF_AP 1
#define WIFI_AUTH_WPA2_PSK 2
#define WIFI_STORAGE_RAM 1
#define WIFI_MODE_AP 1
typedef struct { struct { uint8_t ssid[32],password[64];unsigned ssid_len,channel,authmode,max_connection;struct { bool required; } pmf_cfg; } ap; } wifi_config_t;
typedef struct { bool nvs_enable; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){.nvs_enable=true})
typedef struct { unsigned schan,nchan; } wifi_country_t;
typedef struct { unsigned num;struct { uint8_t mac[6];int rssi; } sta[ESP_WIFI_MAX_CONN_NUM]; } wifi_sta_list_t;
esp_err_t esp_wifi_init(const wifi_init_config_t *);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_set_storage(unsigned);
esp_err_t esp_wifi_set_country_code(const char *,bool);
esp_err_t esp_wifi_get_country(wifi_country_t *);
esp_err_t esp_wifi_set_mode(unsigned);
esp_err_t esp_wifi_set_config(unsigned,const wifi_config_t *);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *);
