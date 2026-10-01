#pragma once

#define AP_SSID "esp32camap"
#define AP_PASSWORD "00000000"

// Start the application SoftAP once, after NVS initialization.
void wifi_ap_start(void);
void wifi_ap_log_clients(void);

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WIFI_AP_CLIENT_CAPACITY 4
typedef struct { uint8_t mac[6]; char ip[16]; int rssi; } wifi_ap_client_t;
/* Only associated clients with a current DHCP lease are returned. */
bool wifi_ap_get_clients(wifi_ap_client_t *out, size_t capacity, size_t *count);

// Select which associated client supplies the camera RSSI; NULL clears it.
void wifi_ap_select_camera(const uint8_t mac[6]);
