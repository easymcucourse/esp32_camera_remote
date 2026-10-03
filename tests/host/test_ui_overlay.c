#include "ui_overlay.h"
#include "camera_menu_navigation.h"
#include <assert.h>
int main(void)
{
    const unsigned order[] = {5, 0, 1, 2, 3, 4, 6, 9, 7, 8};
    for (unsigned i = 0; i < 10; ++i) {
        assert(camera_menu_main_next(order[i], 1) == order[(i + 1) % 10]);
        assert(camera_menu_main_next(order[i], -1) == order[(i + 9) % 10]);
        assert(camera_menu_extra_next(i, 1, 9) == (i + 1) % 10);
        assert(camera_menu_extra_next(i, -1, 9) == (i + 9) % 10);
    }
    uint16_t pixels[12 * 10] = {0};
    ui_overlay_record_border(pixels, 12, 10, false);
    for (unsigned i = 0; i < 120; ++i) assert(pixels[i] == 0);
    ui_overlay_record_border(pixels, 12, 10, true);
    for (unsigned y = 0; y < 10; ++y)
        for (unsigned x = 0; x < 12; ++x)
            assert(pixels[y * 12 + x] == ((y < 4 || y >= 6 || x < 4 || x >= 8) ? 0xf800 : 0));
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
