#include "network_config.h"
#include <string.h>

void network_config_make_default(network_config_t *out)
{
    if (!out) return;
    *out = (network_config_t){.ssid = NETWORK_DEFAULT_SSID, .password = NETWORK_DEFAULT_PASSWORD,
        .channel = NETWORK_DEFAULT_CHANNEL, .show_password = true};
}
bool network_config_uses_default_password(const network_config_t *config)
{ return config && !strcmp(config->password,NETWORK_DEFAULT_PASSWORD); }
network_cfg_error_t network_config_check_ssid(const char *ssid)
{
    if (!ssid) return NETWORK_CFG_INVALID;
    size_t n = 0;
    while (n <= NETWORK_SSID_MAX && ssid[n]) {
        unsigned char c = (unsigned char)ssid[n++];
        if (c < 0x20 || c == 0x7f) return NETWORK_CFG_SSID_BAD_CHAR;
    }
    return !n ? NETWORK_CFG_SSID_EMPTY : n > NETWORK_SSID_MAX ? NETWORK_CFG_SSID_TOO_LONG : NETWORK_CFG_OK;
}
network_cfg_error_t network_config_check_password(const char *password)
{
    if (!password) return NETWORK_CFG_INVALID;
    size_t n = 0;
    while (n <= NETWORK_PASSWORD_MAX && password[n]) {
        unsigned char c = (unsigned char)password[n++];
        if (c < 0x20 || c > 0x7e) return NETWORK_CFG_PASSWORD_BAD_CHAR;
    }
    return n < NETWORK_PASSWORD_MIN ? NETWORK_CFG_PASSWORD_TOO_SHORT :
           n > NETWORK_PASSWORD_MAX ? NETWORK_CFG_PASSWORD_TOO_LONG : NETWORK_CFG_OK;
}
network_cfg_error_t network_config_check_channel(unsigned channel, unsigned max_channel)
{ return channel && channel <= max_channel && channel <= 13 ? NETWORK_CFG_OK : NETWORK_CFG_CHANNEL_RANGE; }
network_cfg_error_t network_config_check(const network_config_t *config, unsigned max_channel)
{
    if (!config) return NETWORK_CFG_INVALID;
    network_cfg_error_t error = network_config_check_ssid(config->ssid);
    if (error == NETWORK_CFG_OK) error = network_config_check_password(config->password);
    if (error == NETWORK_CFG_OK) error = network_config_check_channel(config->channel, max_channel);
    return error;
}
const char *network_config_error_text(network_cfg_error_t error)
{
    static const char *const text[] = {"OK", "SSID must not be empty", "SSID must be 1-32 bytes",
        "SSID contains a control character", "password must be 8-63 characters", "password must be 8-63 characters",
        "password must be printable ASCII", "channel is outside the country range", "invalid configuration"};
    return error <= NETWORK_CFG_INVALID ? text[error] : text[NETWORK_CFG_INVALID];
}
bool network_config_encode(const network_config_t *config, uint8_t out[NETWORK_CONFIG_RECORD_SIZE])
{
    if (!out || network_config_check(config, 13) != NETWORK_CFG_OK) return false;
    size_t ssid = strlen(config->ssid), password = strlen(config->password);
    memset(out, 0, NETWORK_CONFIG_RECORD_SIZE);
    out[0] = 1; out[1] = config->channel; out[2] = config->show_password ? 1 : 0;
    out[3] = (uint8_t)ssid; memcpy(out + 4, config->ssid, ssid);
    out[36] = (uint8_t)password; memcpy(out + 37, config->password, password);
    return true;
}
bool network_config_decode(const uint8_t *bytes, size_t size, network_config_t *out)
{
    if (!bytes || !out || size != NETWORK_CONFIG_RECORD_SIZE || bytes[0] != 1 || (bytes[2] & ~1u) ||
        !bytes[3] || bytes[3] > NETWORK_SSID_MAX || bytes[36] < NETWORK_PASSWORD_MIN || bytes[36] > NETWORK_PASSWORD_MAX) return false;
    network_config_t config = {.channel = bytes[1], .show_password = (bytes[2] & 1) != 0};
    /* Embedded NULs and noncanonical padding cannot silently shorten fields. */
    for (unsigned i = 0; i < 32; ++i) {
        if (i < bytes[3]) { if (!bytes[4 + i]) return false; config.ssid[i] = bytes[4 + i]; }
        else if (bytes[4 + i]) return false;
    }
    for (unsigned i = 0; i < 63; ++i) {
        if (i < bytes[36]) { if (!bytes[37 + i]) return false; config.password[i] = bytes[37 + i]; }
        else if (bytes[37 + i]) return false;
    }
    if (network_config_check(&config, 13) != NETWORK_CFG_OK) return false;
    *out = config; return true;
}
bool network_config_network_equal(const network_config_t *a, const network_config_t *b)
{ return a && b && a->channel == b->channel && !strcmp(a->ssid, b->ssid) && !strcmp(a->password, b->password); }
bool network_config_equal(const network_config_t *a, const network_config_t *b)
{ return network_config_network_equal(a, b) && a->show_password == b->show_password; }
void network_config_make_password(char out[NETWORK_PASSWORD_MAX + 1], network_random_fn random)
{
    static const char alphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
    if (!out) return;
    memset(out, 0, NETWORK_PASSWORD_MAX + 1);
    if (!random) return;
    const unsigned count = sizeof(alphabet) - 1, limit = 256 / count * count;
    for (unsigned i = 0; i < 12;) {
        uint8_t value; random(&value, 1);
        if (value < limit) out[i++] = alphabet[value % count];
    }
}
