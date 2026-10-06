#include "app_core_mode.h"
app_core_mode_t app_core_mode=APP_CORE_MODE_INITIALIZER;
void app_core_mode_boot_begin(app_core_mode_t *mode)
{ if(mode)atomic_store(&mode->booting,true); }
void app_core_mode_boot_end(app_core_mode_t *mode)
{ if(mode)atomic_store(&mode->booting,false); }
bool app_core_mode_booting(const app_core_mode_t *mode)
{ return mode && atomic_load(&mode->booting); }
app_core_mode_value_t app_core_mode_get(const app_core_mode_t *mode)
{ return mode ? (app_core_mode_value_t)atomic_load(&mode->state) : APP_CORE_RESTART; }
bool app_core_mode_enter_normal(app_core_mode_t *mode)
{
    if(!mode)return false;
    unsigned expected=APP_CORE_STARTUP;
    return atomic_compare_exchange_strong(&mode->state,&expected,APP_CORE_NORMAL) || expected==APP_CORE_NORMAL;
}
bool app_core_mode_request_maintenance(app_core_mode_t *mode)
{
    if(!mode)return false;
    unsigned expected=APP_CORE_STARTUP;
    return atomic_compare_exchange_strong(&mode->state,&expected,APP_CORE_ACTIVATING);
}
bool app_core_mode_activate(app_core_mode_t *mode)
{
    if(!mode || app_core_mode_booting(mode))return false;
    unsigned expected=APP_CORE_ACTIVATING;
    return atomic_compare_exchange_strong(&mode->state,&expected,APP_CORE_MAINTENANCE);
}
void app_core_mode_restart(app_core_mode_t *mode)
{ if(mode)atomic_store(&mode->state,APP_CORE_RESTART); }
