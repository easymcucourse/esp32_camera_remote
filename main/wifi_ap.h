#pragma once

#define AP_SSID "esp32camap"
#define AP_PASSWORD "00000000"

// Start the application SoftAP once, after NVS initialization.
void wifi_ap_start(void);
void wifi_ap_log_clients(void);
