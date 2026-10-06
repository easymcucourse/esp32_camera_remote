#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ui_wifi_menu_kernel.h"
static wifi_menu_t m;
static wifi_menu_effect_t input(wifi_menu_input_t i, int d, uint32_t now)
{ return wifi_menu_input(&m, i, d, now); }
int main(void)
{
    network_config_t config; network_config_make_default(&config);
    wifi_menu_open(&m, &config, 11);
    assert(input(WIFI_MENU_CONFIRM, 0, 0) == WIFI_MENU_NONE && m.editing);
    input(WIFI_MENU_MOVE, 1, 0); assert(m.edit[0] == 'f');
    input(WIFI_MENU_BACK, 0, 0); assert(!m.editing && !strcmp(m.draft.ssid, config.ssid));
    input(WIFI_MENU_CONFIRM, 0, 0); input(WIFI_MENU_MOVE, -1, 0);
    input(WIFI_MENU_CONFIRM, 0, 0); assert(!m.editing && m.draft.ssid[0] == 'd');
    assert(config.ssid[0] == 'e' && m.actual.ssid[0] == 'e');
    m.row = WIFI_ROW_APPLY;
    assert(input(WIFI_MENU_CONFIRM, 0, 0) == WIFI_MENU_APPLY);
    m.pending = true; input(WIFI_MENU_MOVE, 1, 0); assert(m.row == WIFI_ROW_APPLY);
    wifi_menu_complete(&m, &config, false, false); assert(m.draft.ssid[0] == 'd' && !m.pending);
    m.row = WIFI_ROW_CHANNEL; m.draft.channel = 11;
    input(WIFI_MENU_STEP, 1, 0); assert(m.draft.channel == 11);
    m.draft.channel = 1; input(WIFI_MENU_STEP, -1, 0); assert(m.draft.channel == 1);
    m.row = WIFI_ROW_SHOW;
    assert(input(WIFI_MENU_STEP, 1, 0) == WIFI_MENU_DISPLAY);
    assert(!m.draft.show_password && m.actual.show_password);
    config.show_password = false;
    wifi_menu_complete(&m, &config, true, true);
    assert(m.draft.ssid[0] == 'd' && !m.draft.show_password);
    m.row = WIFI_ROW_RESET;
    input(WIFI_MENU_CONFIRM, 0, 1); assert(m.confirm_reset);
    input(WIFI_MENU_MOVE, 1, 2); assert(!m.confirm_reset);
    m.row = WIFI_ROW_RESET;
    input(WIFI_MENU_CONFIRM, 0, UINT32_MAX - 1000);
    wifi_menu_tick(&m, 1998); assert(m.confirm_reset);
    wifi_menu_tick(&m, 1999); assert(!m.confirm_reset);
    input(WIFI_MENU_CONFIRM, 0, 2000);
    assert(input(WIFI_MENU_CONFIRM, 0, 4999) == WIFI_MENU_APPLY);
    assert(!strcmp(m.draft.ssid, NETWORK_DEFAULT_SSID) && m.draft.channel == 6 && m.draft.show_password);
    m.row = WIFI_ROW_RESET_ALL;
    assert(input(WIFI_MENU_CONFIRM, 0, 0) == WIFI_MENU_NONE && m.confirm_reset);
    assert(input(WIFI_MENU_CONFIRM, 0, 2999) == WIFI_MENU_RESET_ALL && !m.confirm_reset);
    input(WIFI_MENU_CONFIRM, 0, 3000); wifi_menu_tick(&m, 6000); assert(!m.confirm_reset);
    assert(input(WIFI_MENU_CONFIRM, 0, 6001) == WIFI_MENU_NONE);
    input(WIFI_MENU_STEP, 1, 6002); assert(!m.confirm_reset);
    m.row = WIFI_ROW_NEWPASS; assert(input(WIFI_MENU_CONFIRM, 0, 0) == WIFI_MENU_RANDOM);
    /* Cursor reaches the append position, and never writes beyond 32 bytes. */
    memset(config.ssid, 'a', 31); config.ssid[31] = 0;
    wifi_menu_open(&m, &config, 13); input(WIFI_MENU_CONFIRM, 0, 0);
    for (unsigned i = 0; i < 50; ++i) input(WIFI_MENU_STEP, 1, 0);
    assert(m.cursor == 31); input(WIFI_MENU_MOVE, 1, 0); assert(strlen(m.edit) == 32);
    input(WIFI_MENU_STEP, 1, 0); assert(m.cursor == 31);
    input(WIFI_MENU_MOVE, -1, 0); assert(strlen(m.edit) == 31);
    for (unsigned i = 0; i < 50; ++i) input(WIFI_MENU_STEP, -1, 0);
    input(WIFI_MENU_MOVE, -1, 0); assert(m.edit[0] == 0);
    input(WIFI_MENU_CONFIRM, 0, 0); assert(m.editing); /* Empty SSID cannot finish. */
    input(WIFI_MENU_MOVE, 1, 0); input(WIFI_MENU_CONFIRM, 0, 0);
    assert(!m.editing && !strcmp(m.draft.ssid, "a"));
    m.pending = true;
    assert(input(WIFI_MENU_BACK, 0, 0) == WIFI_MENU_CLOSE && !m.active);
    assert(input(WIFI_MENU_CONFIRM, 0, 0) == WIFI_MENU_NONE);
    puts("Wi-Fi draft, editor, apply and confirmation tests passed");
    return 0;
}
