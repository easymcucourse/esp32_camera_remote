#pragma once
#include "app_wifi.h"
typedef void (*wifi_tcp_network_fn)(void *context, uint32_t *generation, bool *online);
typedef void (*wifi_tcp_diagnostic_fn)(void *context, int error);
app_wifi_result_t wifi_tcp_open(void *context, wifi_tcp_network_fn network,
    wifi_tcp_diagnostic_fn diagnostic, const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline, void **channel);
app_wifi_result_t wifi_tcp_io(void *channel, void *data, size_t size, bool transmit,
    const app_wifi_deadline_t *deadline, size_t *bytes);
void wifi_tcp_close(void *channel);
app_wifi_result_t wifi_tcp_poll(void *channel, const app_wifi_deadline_t *deadline, bool *readable);
