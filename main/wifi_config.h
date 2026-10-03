#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define WIFI_SSID_MAX 32
#define WIFI_PASSWORD_MIN 8
#define WIFI_PASSWORD_MAX 63
#define WIFI_CONFIG_RECORD_SIZE 100
#define WIFI_DEFAULT_SSID "easycamctrl"
#define WIFI_DEFAULT_PASSWORD "00000000"
#define WIFI_DEFAULT_CHANNEL 6
typedef struct {
    char ssid[WIFI_SSID_MAX + 1], password[WIFI_PASSWORD_MAX + 1];
    uint8_t channel;
    bool show_password;
} app_wifi_config_t;
typedef enum {
    WIFI_CFG_OK, WIFI_CFG_SSID_EMPTY, WIFI_CFG_SSID_TOO_LONG, WIFI_CFG_SSID_BAD_CHAR,
    WIFI_CFG_PASSWORD_TOO_SHORT, WIFI_CFG_PASSWORD_TOO_LONG, WIFI_CFG_PASSWORD_BAD_CHAR,
    WIFI_CFG_CHANNEL_RANGE, WIFI_CFG_INVALID
} wifi_cfg_error_t;
void wifi_config_make_default(app_wifi_config_t *out);
bool wifi_config_uses_default_password(const app_wifi_config_t *config);
wifi_cfg_error_t wifi_config_check_ssid(const char *ssid);
wifi_cfg_error_t wifi_config_check_password(const char *password);
wifi_cfg_error_t wifi_config_check_channel(unsigned channel, unsigned max_channel);
wifi_cfg_error_t wifi_config_check(const app_wifi_config_t *config, unsigned max_channel);
const char *wifi_config_error_text(wifi_cfg_error_t error);
bool wifi_config_encode(const app_wifi_config_t *config, uint8_t out[WIFI_CONFIG_RECORD_SIZE]);
bool wifi_config_decode(const uint8_t *bytes, size_t size, app_wifi_config_t *out);
bool wifi_config_equal(const app_wifi_config_t *a, const app_wifi_config_t *b);
bool wifi_config_network_equal(const app_wifi_config_t *a, const app_wifi_config_t *b);
typedef void (*wifi_random_fn)(void *buffer, size_t length);
void wifi_config_make_password(char out[WIFI_PASSWORD_MAX + 1], wifi_random_fn random);
