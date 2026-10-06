#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct esp_netif { int identity; } esp_netif_t;
typedef struct { struct { uint32_t addr; } ip; } esp_netif_ip_info_t;
esp_netif_t *esp_netif_get_handle_from_ifkey(const char *key);
bool esp_netif_is_netif_up(esp_netif_t *netif);
esp_err_t esp_netif_get_ip_info(esp_netif_t *netif,esp_netif_ip_info_t *out);
esp_err_t esp_netif_get_netif_impl_name(esp_netif_t *netif,char *name);
