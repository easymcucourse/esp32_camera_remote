#include "ui_wifi_menu_kernel_noreset.h"
#include <string.h>

void wifi_menu_open(wifi_menu_t *m, const network_config_t *config, unsigned max_channel)
{
    memset(m, 0, sizeof(*m)); m->actual = m->draft = *config;
    m->max_channel = max_channel; m->active = true;
}
void wifi_menu_tick(wifi_menu_t *m, uint32_t now)
{
    (void)m;(void)now;
}
static void edit_step(wifi_menu_t *m, int direction)
{
    /* Empty terminator is selectable: it deletes this character and the suffix. */
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyz0123456789-_";
    unsigned length = (unsigned)strlen(m->edit), index = sizeof(alphabet) - 1;
    const char *found = m->edit[m->cursor] ? strchr(alphabet, m->edit[m->cursor]) : NULL;
    if (found) index = (unsigned)(found - alphabet);
    index = (index + (direction > 0 ? 1 : sizeof(alphabet) - 1)) % sizeof(alphabet);
    m->edit[m->cursor] = alphabet[index];
    if (m->cursor == length && alphabet[index]) m->edit[m->cursor + 1] = 0;
}
wifi_menu_effect_t wifi_menu_input(wifi_menu_t *m, wifi_menu_input_t input, int direction, uint32_t now)
{
    wifi_menu_tick(m, now);
    if (!m->active) return WIFI_MENU_NONE;
    if (input == WIFI_MENU_BACK) {
        if (m->editing) { m->editing = false; return WIFI_MENU_NONE; }
        m->active = false; return WIFI_MENU_CLOSE;
    }
    if (m->pending) return WIFI_MENU_NONE;
    if (m->editing) {
        if (input == WIFI_MENU_CONFIRM) {
            if (network_config_check_ssid(m->edit) == NETWORK_CFG_OK) {
                memcpy(m->draft.ssid, m->edit, sizeof(m->edit)); m->editing = false;
            }
        } else if (direction == -1 || direction == 1) {
            if (input == WIFI_MENU_MOVE) edit_step(m, direction);
            else if (input == WIFI_MENU_STEP) {
                unsigned last = (unsigned)strlen(m->edit);
                if (last >= NETWORK_SSID_MAX) last = NETWORK_SSID_MAX - 1;
                if (direction < 0 && m->cursor) --m->cursor;
                if (direction > 0 && m->cursor < last) ++m->cursor;
            }
        }
        return WIFI_MENU_NONE;
    }
    if (input == WIFI_MENU_MOVE && (direction == -1 || direction == 1)) {
        m->row = (m->row + (direction > 0 ? 1 : WIFI_ROW_COUNT - 1)) % WIFI_ROW_COUNT;
    } else if (input == WIFI_MENU_STEP && (direction == -1 || direction == 1)) {
        if (m->row == WIFI_ROW_CHANNEL) {
            int channel = m->draft.channel + direction;
            if (channel >= 1 && (unsigned)channel <= m->max_channel) m->draft.channel = (uint8_t)channel;
        } else if (m->row == WIFI_ROW_SHOW) {
            m->draft.show_password = !m->actual.show_password;
            return WIFI_MENU_DISPLAY;
        }
    } else if (input == WIFI_MENU_CONFIRM) {
        switch (m->row) {
        case WIFI_ROW_SSID:
            memcpy(m->edit, m->draft.ssid, sizeof(m->edit)); m->cursor = 0; m->editing = true; break;
        case WIFI_ROW_NEWPASS: return WIFI_MENU_RANDOM;
        case WIFI_ROW_APPLY:
            if (!network_config_equal(&m->actual, &m->draft)) return WIFI_MENU_APPLY;
            break;
        case WIFI_ROW_BACK: m->active = false; return WIFI_MENU_CLOSE;
        default: break;
        }
    }
    return WIFI_MENU_NONE;
}
void wifi_menu_complete(wifi_menu_t *m, const network_config_t *actual, bool success, bool display_only)
{
    m->pending = false; m->actual = *actual;
    if (display_only) m->draft.show_password = actual->show_password;
    else if (success) m->draft = *actual;
}
