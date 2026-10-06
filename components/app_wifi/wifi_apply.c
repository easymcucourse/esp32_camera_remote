#include "wifi_apply.h"
wifi_apply_result_t wifi_apply_config(network_config_t *current, const network_config_t *next,
                                      unsigned max_channel, wifi_apply_fn save, wifi_apply_fn restart, void *context)
{
    if (!current || !save || !restart || network_config_check(next, max_channel) != NETWORK_CFG_OK) return WIFI_APPLY_INVALID;
    if (network_config_equal(current, next)) return WIFI_APPLY_OK;
    if (!save(context, next)) return WIFI_APPLY_SAVE_FAILED;
    if (!network_config_network_equal(current, next) && !restart(context, next)) {
        bool saved = save(context, current);
        bool restored = restart(context, current);
        return saved && restored ? WIFI_APPLY_DRIVER_FAILED : WIFI_APPLY_ROLLBACK_FAILED;
    }
    *current = *next; return WIFI_APPLY_OK;
}
