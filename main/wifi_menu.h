#pragma once
#include "wifi_config.h"

enum { WIFI_ROW_SSID, WIFI_ROW_PASS, WIFI_ROW_NEWPASS, WIFI_ROW_CHANNEL,
       WIFI_ROW_SHOW, WIFI_ROW_APPLY, WIFI_ROW_RESET, WIFI_ROW_RESET_ALL,
       WIFI_ROW_BACK, WIFI_ROW_COUNT };
typedef enum { WIFI_MENU_MOVE, WIFI_MENU_STEP, WIFI_MENU_CONFIRM, WIFI_MENU_BACK } wifi_menu_input_t;
typedef enum { WIFI_MENU_NONE, WIFI_MENU_APPLY, WIFI_MENU_DISPLAY, WIFI_MENU_RANDOM,
               WIFI_MENU_CLOSE, WIFI_MENU_RESET_ALL } wifi_menu_effect_t;
typedef struct {
    app_wifi_config_t actual, draft;
    char edit[WIFI_SSID_MAX + 1];
    unsigned row, cursor, max_channel;
    bool active, editing, pending, confirm_reset;
    uint32_t confirm_deadline;
} wifi_menu_t;
void wifi_menu_open(wifi_menu_t *menu, const app_wifi_config_t *config, unsigned max_channel);
void wifi_menu_tick(wifi_menu_t *menu, uint32_t now);
wifi_menu_effect_t wifi_menu_input(wifi_menu_t *menu, wifi_menu_input_t input, int direction, uint32_t now);
void wifi_menu_complete(wifi_menu_t *menu, const app_wifi_config_t *actual, bool success, bool display_only);
