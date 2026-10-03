#include "ui_overlay.h"
const char *ui_info_name(unsigned level)
{
    static const char *const names[]={"full","compact","hidden"};
    return level<=UI_INFO_HIDDEN?names[level]:"invalid";
}
ui_overlay_policy_t ui_overlay_policy(unsigned level,unsigned battery,bool recording)
{
    if (level>UI_INFO_HIDDEN) level=UI_INFO_FULL;
    return (ui_overlay_policy_t){.status_bar=level==UI_INFO_FULL,
        .battery_warning=level!=UI_INFO_HIDDEN && battery<=20,
        .record_dot=recording,.record_text=recording && level!=UI_INFO_HIDDEN,
        .command_errors=level!=UI_INFO_HIDDEN};
}
