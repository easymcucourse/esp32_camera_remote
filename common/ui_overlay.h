#pragma once
#include <stdbool.h>
typedef enum { UI_INFO_FULL, UI_INFO_COMPACT, UI_INFO_HIDDEN } ui_info_level_t;
typedef struct { bool status_bar, battery_warning, record_dot, record_text, command_errors; } ui_overlay_policy_t;
const char *ui_info_name(unsigned level);
ui_overlay_policy_t ui_overlay_policy(unsigned level,unsigned battery,bool recording);
