#include "ui_render_lifecycle.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
static atomic_bool closed,refresh_running;
static atomic_uint users;
bool ui_render_enter(void)
{
    atomic_fetch_add(&users,1);
    if (!atomic_load(&closed)) return true;
    atomic_fetch_sub(&users,1);return false;
}
void ui_render_leave(void) { atomic_fetch_sub(&users,1); }
bool ui_render_stopping(void) { return atomic_load(&closed); }
bool ui_render_idle(void) { return !atomic_load(&users); }
void ui_render_refresh_set(bool active) { atomic_store(&refresh_running,active); }
bool ui_render_close(int64_t deadline)
{
    atomic_store(&closed,true);
    while (atomic_load(&users) || atomic_load(&refresh_running)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}
