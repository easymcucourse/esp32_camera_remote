#include "ui_preferences.h"
#include "preferences_store.h"
#include "app_ui_internal.h"
#include "esp_timer.h"
#include <stdatomic.h>

static atomic_uint level,pad_type;
static atomic_bool ready;
unsigned ui_preferences_level(void) { return atomic_load(&level); }
unsigned ui_preferences_pad(void) { return atomic_load(&pad_type); }
esp_err_t ui_preferences_start(void)
{
    if (atomic_load(&ready)) return ESP_OK;
    preferences_config_t saved={PREFERENCES_CONFIG_VERSION,0,0};
    /* Preserve old boot fallback on unavailable/future records; loader leaves
     * output unchanged and never rewrites them. All writes are maintenance. */
    (void)preferences_store_load(&saved);
    atomic_store(&level,saved.info_level);atomic_store(&pad_type,saved.controller);
    app_ui_set_info_level(saved.info_level);atomic_store(&ready,true);return ESP_OK;
}
bool ui_preferences_quiesce(uint32_t timeout_ms)
{ (void)timeout_ms;atomic_store(&ready,false);return true; }
esp_err_t ui_preferences_message(const app_message_t *m,app_message_t *reply,bool *deferred)
{
    if (!m || !reply || !deferred || m->lease || m->type!=APP_MESSAGE_UI_PREFERENCES ||
        (m->flags!=0 && m->flags!=APP_MESSAGE_REQUEST) || !m->generation ||
        (m->source!=APP_ENDPOINT_INPUT && m->source!=APP_ENDPOINT_UART &&
         m->source!=APP_ENDPOINT_SYSTEM && m->source!=APP_ENDPOINT_INPUT_ATOM)) return ESP_ERR_INVALID_ARG;
    *deferred=false;
    if (!atomic_load(&ready)) return ESP_ERR_INVALID_STATE;
    if (m->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (m->payload.command.index!=APP_UI_PREF_GET) return ESP_ERR_NOT_SUPPORTED;
    if (m->payload.command.flag) return ESP_ERR_INVALID_ARG;
    reply->payload.command.value=ui_preferences_level();
    reply->payload.command.direction=(int32_t)ui_preferences_pad();return ESP_OK;
}
