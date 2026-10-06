#pragma once
#include "app_wifi.h"
#include "app_message.h"
esp_err_t wifi_channel_messages_start(app_wifi_t *wifi);
/* Dispatch consumes the message only on success; failures leave its lease. */
esp_err_t wifi_channel_messages_dispatch(app_message_t *message);
void wifi_channel_messages_cancel(void);
bool wifi_channel_messages_idle(void);
void wifi_channel_messages_cleanup(void);
