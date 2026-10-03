#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "wifi_config.h"
static unsigned random_calls;
static void fake_random(void *out, size_t size)
{ assert(size == 1); *(uint8_t *)out = random_calls++ % 2 ? 0 : 255; }
int main(void)
{
    app_wifi_config_t config, decoded;
    wifi_config_make_default(&config);
    assert(!strcmp(config.ssid, "easycamctrl") && !strcmp(config.password, "00000000") && config.channel == 6 && config.show_password);
    assert(wifi_config_check(&config, 13) == WIFI_CFG_OK);
    assert(wifi_config_uses_default_password(&config));
    app_wifi_config_t hidden=config;hidden.show_password=false;strcpy(hidden.ssid,"custom");
    assert(wifi_config_uses_default_password(&hidden));
    strcpy(hidden.password,"changed-pass");assert(!wifi_config_uses_default_password(&hidden));
    assert(wifi_config_check_ssid("") == WIFI_CFG_SSID_EMPTY);
    char text[66]; memset(text, 'a', sizeof(text)); text[32] = 0;
    assert(wifi_config_check_ssid(text) == WIFI_CFG_OK); text[32] = 'a'; text[33] = 0;
    assert(wifi_config_check_ssid(text) == WIFI_CFG_SSID_TOO_LONG);
    assert(wifi_config_check_ssid("a\x1f") == WIFI_CFG_SSID_BAD_CHAR);
    assert(wifi_config_check_ssid("a\x7f") == WIFI_CFG_SSID_BAD_CHAR);
    assert(wifi_config_check_ssid("\xe6\x91\x84\xe5\x83\x8f\xe6\x9c\xba") == WIFI_CFG_OK);
    memset(text, ' ', sizeof(text)); text[7] = 0;
    assert(wifi_config_check_password(text) == WIFI_CFG_PASSWORD_TOO_SHORT);
    text[7] = ' '; text[8] = 0; assert(wifi_config_check_password(text) == WIFI_CFG_OK);
    memset(text, 'a', sizeof(text)); text[63] = 0; assert(wifi_config_check_password(text) == WIFI_CFG_OK);
    text[63] = 'a'; text[64] = 0; assert(wifi_config_check_password(text) == WIFI_CFG_PASSWORD_TOO_LONG);
    assert(wifi_config_check_password("abcdefg\x7f") == WIFI_CFG_PASSWORD_BAD_CHAR);
    assert(wifi_config_check_password("abcdefg\x80") == WIFI_CFG_PASSWORD_BAD_CHAR);
    assert(wifi_config_check_channel(1, 11) == WIFI_CFG_OK && wifi_config_check_channel(11, 11) == WIFI_CFG_OK);
    assert(wifi_config_check_channel(0, 13) == WIFI_CFG_CHANNEL_RANGE && wifi_config_check_channel(12, 11) == WIFI_CFG_CHANNEL_RANGE);
    assert(wifi_config_check_channel(13, 13) == WIFI_CFG_OK && wifi_config_check_channel(14, 255) == WIFI_CFG_CHANNEL_RANGE);
    uint8_t bytes[WIFI_CONFIG_RECORD_SIZE], original[WIFI_CONFIG_RECORD_SIZE];
    assert(wifi_config_encode(&config, bytes)); memcpy(original, bytes, sizeof(bytes));
    assert(wifi_config_decode(bytes, sizeof(bytes), &decoded) && wifi_config_equal(&config, &decoded));
    for (size_t n = 0; n < sizeof(bytes); ++n) assert(!wifi_config_decode(bytes, n, &decoded));
    bytes[0] = 2; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[2] = 2; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[3] = 33; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[36] = 64; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[4] = 0; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[35] = 1; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    memcpy(bytes, original, sizeof(bytes)); bytes[99] = 1; assert(!wifi_config_decode(bytes, sizeof(bytes), &decoded));
    wifi_config_make_password(config.password, fake_random);
    assert(strlen(config.password) == 12 && random_calls == 24);
    assert(!strcmp(config.password, "aaaaaaaaaaaa"));
    assert(wifi_config_check(&config, 13) == WIFI_CFG_OK);
    puts("Wi-Fi config boundaries, records and random rejection tests passed");
}
