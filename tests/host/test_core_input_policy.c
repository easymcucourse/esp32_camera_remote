#include "app_core.h"
#include "app_core_services.h"
#include "sdkconfig.h"
#include <assert.h>
static int64_t clock_us;
static esp_err_t atom_start_result=ESP_OK,sim_start_result=ESP_OK,sim_stop_result=ESP_OK;
static unsigned atom_starts,sim_starts,atom_stops,sim_stops;
static uint32_t atom_budget;
esp_err_t input_atom_prepare(void) { return atom_start_result; }
esp_err_t input_atom_start(void) { ++atom_starts;return atom_start_result; }
esp_err_t input_sim_start(void) { ++sim_starts;return sim_start_result; }
esp_err_t input_atom_stop(uint32_t timeout) { ++atom_stops;atom_budget=timeout;return ESP_OK; }
esp_err_t input_sim_stop(uint32_t timeout)
{ assert(timeout==400);++sim_stops;clock_us+=170000;return sim_stop_result; }
int64_t esp_timer_get_time(void) { return clock_us; }
int main(void)
{
    atom_start_result=ESP_ERR_NO_MEM;
    assert(app_core_input_providers_prepare()==ESP_ERR_NO_MEM && !atom_starts && !sim_starts);
    assert(app_core_input_providers_start()==ESP_ERR_NO_MEM && atom_starts==1 && !sim_starts);
    atom_start_result=ESP_OK;
    assert(app_core_input_providers_prepare()==ESP_OK && atom_starts==1 && !sim_starts);
#if CONFIG_REMOTE_DBG_SIM
    sim_start_result=ESP_ERR_NO_MEM;
    assert(app_core_input_providers_start()==ESP_ERR_NO_MEM && sim_starts==1 && atom_stops==1 && atom_budget==1000);
    sim_start_result=ESP_OK;
    assert(app_core_input_providers_start()==ESP_OK && sim_starts==2);
    assert(app_core_input_providers_stop(400)==ESP_OK && sim_stops==1 && atom_budget==230);
    sim_stop_result=ESP_ERR_TIMEOUT;
    assert(app_core_input_providers_stop(400)==ESP_ERR_TIMEOUT && sim_stops==2 && atom_stops==3 && atom_budget==230);
#else
    assert(app_core_input_providers_start()==ESP_OK && !sim_starts);
    assert(app_core_input_providers_stop(400)==ESP_OK && atom_stops==1 && atom_budget==400 && !sim_stops);
#endif
}
