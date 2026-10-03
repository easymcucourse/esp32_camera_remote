#include "ui_overlay.h"
void ui_overlay_record_border(uint16_t *pixels, unsigned width, unsigned height, bool recording)
{
    if (!recording || !pixels || width < 8 || height < 8) return;
    for (unsigned edge = 0; edge < 4; ++edge) {
        for (unsigned x = 0; x < width; ++x) {
            pixels[edge * width + x] = 0xf800;
            pixels[(height - 1 - edge) * width + x] = 0xf800;
        }
        for (unsigned y = 4; y < height - 4; ++y) {
            pixels[y * width + edge] = 0xf800;
            pixels[y * width + width - 1 - edge] = 0xf800;
        }
    }
}
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
