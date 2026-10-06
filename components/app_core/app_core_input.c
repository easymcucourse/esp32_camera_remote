#include "app_core.h"
#include "app_core_services.h"
#include "input_atom.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#if CONFIG_REMOTE_DBG_SIM
#include "input_sim.h"
#endif
esp_err_t app_core_input_providers_prepare(void) { return input_atom_prepare(); }
esp_err_t app_core_input_providers_start(void)
{
    esp_err_t err=input_atom_start();if (err!=ESP_OK) return err;
#if CONFIG_REMOTE_DBG_SIM
    err=input_sim_start();
    if (err!=ESP_OK) input_atom_stop(1000);
#endif
    return err;
}
esp_err_t app_core_input_providers_stop(uint32_t timeout_ms)
{
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    esp_err_t result=ESP_OK;
#if CONFIG_REMOTE_DBG_SIM
    result=input_sim_stop(timeout_ms);
#endif
    int64_t remaining=deadline-esp_timer_get_time();
    esp_err_t err=input_atom_stop(remaining>0?(uint32_t)(remaining/1000):0);
    return result==ESP_OK?err:result;
}
