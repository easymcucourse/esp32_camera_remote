#pragma once

#include "wifi_config.h"
#include "esp_err.h"
#include <stdint.h>

// Start the application SoftAP once, after NVS initialization.
void wifi_ap_start(void);
void wifi_ap_log_clients(void);
void wifi_ap_load_config(void);
void wifi_ap_get_config(app_wifi_config_t *out);
unsigned wifi_ap_max_channel(void);
/* Copied nonblocking requests; only the worker writes NVS/restarts Wi-Fi.
 * Query completion by token. NOT_FINISHED means pending, NOT_FOUND means expired. */
esp_err_t wifi_ap_request_apply(const app_wifi_config_t *config, uint32_t *token);
/* HTTP two-phase handoff: reserve first; after response, commit with a bounded
 * delay. Failed sends cancel without changing NVS or the running network. */
esp_err_t wifi_ap_prepare_apply(const app_wifi_config_t *config,uint32_t *token);
esp_err_t wifi_ap_commit_apply(uint32_t token,unsigned delay_ms);
void wifi_ap_cancel_apply(uint32_t token);
uint32_t wifi_ap_network_generation(void);
/* Full reset stops the camera and reboots on success; confirm at the caller. */
esp_err_t wifi_ap_request_reset(bool all, uint32_t *token);
esp_err_t wifi_ap_request_result(uint32_t token, esp_err_t *result);

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WIFI_AP_CLIENT_CAPACITY 4
typedef struct { uint8_t mac[6]; char ip[16]; int rssi; } wifi_ap_client_t;
/* Only associated clients with a current DHCP lease are returned. */
bool wifi_ap_get_clients(wifi_ap_client_t *out, size_t capacity, size_t *count);

// Select which associated client supplies the camera RSSI; NULL clears it.
void wifi_ap_select_camera(const uint8_t mac[6]);
