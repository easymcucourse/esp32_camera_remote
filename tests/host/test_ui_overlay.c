#include "ui_overlay.h"
#include <assert.h>
int main(void)
{
    for (unsigned mode=0;mode<3;++mode) {
        ui_overlay_policy_t p=ui_overlay_policy(mode,255,true);
        assert(p.record_dot && !p.battery_warning);
        assert(p.record_text==(mode!=UI_INFO_HIDDEN));
        assert(p.status_bar==(mode==UI_INFO_FULL));
        assert(p.command_errors==(mode!=UI_INFO_HIDDEN));
        p=ui_overlay_policy(mode,20,false);
        assert(p.battery_warning==(mode!=UI_INFO_HIDDEN) && !p.record_dot && !p.record_text);
    }
    assert(ui_overlay_policy(255,255,false).status_bar);
    assert(!ui_overlay_policy(UI_INFO_COMPACT,21,false).battery_warning);
    return 0;
}
