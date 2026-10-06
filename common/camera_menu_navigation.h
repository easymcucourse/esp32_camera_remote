#pragma once
/* Stable property IDs; screen order puts Focus before Shutter and More before Wi-Fi. */
static inline unsigned camera_menu_main_next(unsigned selected, int direction)
{
    static const unsigned order[9] = {5, 0, 1, 2, 3, 4, 6, 9, 7};
    unsigned position = 0;
    while (position < 8 && order[position] != selected) ++position;
    if (direction != 1 && direction != -1) return selected;
    return order[(position + (direction > 0 ? 1 : 8)) % 9];
}
static inline unsigned camera_menu_extra_next(unsigned selected, int direction, unsigned count)
{
    if (direction != 1 && direction != -1) return selected;
    return (selected + (direction > 0 ? 1 : count)) % (count + 1);
}
