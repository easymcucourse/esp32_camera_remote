#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>
typedef struct { unsigned alive; } esp_netif_t;
typedef struct { uint32_t addr; } esp_ip4_addr_t;
typedef struct { esp_ip4_addr_t ip; } esp_netif_ip_info_t;
typedef struct { uint8_t mac[6];esp_ip4_addr_t ip; } esp_netif_pair_mac_ip_t;
#define IPSTR "%u.%u.%u.%u"
#define IP2STR(p) (unsigned)((p)->addr&255),(unsigned)(((p)->addr>>8)&255),(unsigned)(((p)->addr>>16)&255),(unsigned)(((p)->addr>>24)&255)
esp_err_t esp_netif_init(void);
esp_netif_t *esp_netif_create_default_wifi_ap(void);
void esp_netif_destroy_default_wifi(esp_netif_t *);
bool esp_netif_is_netif_up(esp_netif_t *);
esp_err_t esp_netif_get_ip_info(esp_netif_t *,esp_netif_ip_info_t *);
esp_err_t esp_netif_dhcps_get_clients_by_mac(esp_netif_t *,unsigned,esp_netif_pair_mac_ip_t *);
