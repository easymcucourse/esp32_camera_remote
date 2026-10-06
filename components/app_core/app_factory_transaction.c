#include "app_core_factory.h"
factory_reset_result_t factory_reset_all(const network_config_t *current,
                                        const factory_reset_ops_t *ops, void *context)
{
    if (!current || !ops || !ops->acquire_camera || !ops->release_camera ||
        !ops->save_wifi || !ops->forget_camera || !ops->reset_ui) return FACTORY_RESET_INVALID;
    if (!ops->acquire_camera(context)) return FACTORY_RESET_BUSY;
    network_config_t defaults; network_config_make_default(&defaults);
    bool saved = ops->save_wifi(context, &defaults);
    bool erased = saved && ops->forget_camera(context);
    bool ui_reset=erased && ops->reset_ui(context);
    if (saved && erased && ui_reset) return FACTORY_RESET_OK;
    bool rolled_back = ops->save_wifi(context, current);
    ops->release_camera(context);
    if (!rolled_back) return FACTORY_RESET_ROLLBACK_FAILED;
    return !saved?FACTORY_RESET_SAVE_FAILED:!erased?FACTORY_RESET_IDENTITY_FAILED:FACTORY_RESET_UI_FAILED;
}
