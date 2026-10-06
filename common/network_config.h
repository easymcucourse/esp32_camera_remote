#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NETWORK_SSID_MAX 32
#define NETWORK_PASSWORD_MIN 8
#define NETWORK_PASSWORD_MAX 63
#define NETWORK_CONFIG_RECORD_SIZE 100
#define NETWORK_DEFAULT_SSID "easycamctrl"
#define NETWORK_DEFAULT_PASSWORD "00000000"
#define NETWORK_DEFAULT_CHANNEL 6
typedef struct {
    char ssid[NETWORK_SSID_MAX + 1], password[NETWORK_PASSWORD_MAX + 1];
    uint8_t channel;
    bool show_password;
} network_config_t;
typedef enum {
    NETWORK_CFG_OK, NETWORK_CFG_SSID_EMPTY, NETWORK_CFG_SSID_TOO_LONG, NETWORK_CFG_SSID_BAD_CHAR,
    NETWORK_CFG_PASSWORD_TOO_SHORT, NETWORK_CFG_PASSWORD_TOO_LONG, NETWORK_CFG_PASSWORD_BAD_CHAR,
    NETWORK_CFG_CHANNEL_RANGE, NETWORK_CFG_INVALID
} network_cfg_error_t;
void network_config_make_default(network_config_t *out);
bool network_config_uses_default_password(const network_config_t *config);
network_cfg_error_t network_config_check_ssid(const char *ssid);
network_cfg_error_t network_config_check_password(const char *password);
network_cfg_error_t network_config_check_channel(unsigned channel, unsigned max_channel);
network_cfg_error_t network_config_check(const network_config_t *config, unsigned max_channel);
const char *network_config_error_text(network_cfg_error_t error);
bool network_config_encode(const network_config_t *config, uint8_t out[NETWORK_CONFIG_RECORD_SIZE]);
bool network_config_decode(const uint8_t *bytes, size_t size, network_config_t *out);
bool network_config_equal(const network_config_t *a, const network_config_t *b);
bool network_config_network_equal(const network_config_t *a, const network_config_t *b);
typedef void (*network_random_fn)(void *buffer, size_t length);
void network_config_make_password(char out[NETWORK_PASSWORD_MAX + 1], network_random_fn random);
