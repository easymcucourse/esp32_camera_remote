#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ui_wifi_menu_kernel_noreset.h"
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
    assert(WIFI_ROW_COUNT==7 && WIFI_ROW_BACK==6);
    m.row=WIFI_ROW_BACK;assert(input(WIFI_MENU_CONFIRM,0,0)==WIFI_MENU_CLOSE && !m.active);
    wifi_menu_open(&m,&config,11);
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
    puts("Legacy Wi-Fi draft, editor and retired reset rows tests passed (test-only source)");
    return 0;
}
