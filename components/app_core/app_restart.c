#include "app_restart.h"
#include "restart_schedule.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static restart_schedule_t schedule;
bool app_restart_prepare(void)
{ portENTER_CRITICAL(&mux);bool ok=restart_schedule_prepare(&schedule);portEXIT_CRITICAL(&mux);return ok; }
bool app_restart_commit(unsigned delay)
{
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    portENTER_CRITICAL(&mux);bool ok=restart_schedule_commit(&schedule,now,delay);portEXIT_CRITICAL(&mux);return ok;
}
void app_restart_cancel(void)
{ portENTER_CRITICAL(&mux);restart_schedule_cancel(&schedule);portEXIT_CRITICAL(&mux); }
bool app_restart_due(void)
{
    uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
    portENTER_CRITICAL(&mux);bool due=restart_schedule_due(&schedule,now);portEXIT_CRITICAL(&mux);return due;
}
bool app_restart_pending(void)
{ portENTER_CRITICAL(&mux);bool committed=schedule.committed;portEXIT_CRITICAL(&mux);return committed; }
