#include "app_core.h"
#include "app_core_services.h"
#include "app_core_maintenance.h"
#include "app_core_mode.h"
#include "app_core_shutdown.h"
#include "app_maintenance.h"
#include "app_maintenance_web.h"
#include "app_maintenance_ota.h"
#include "app_core_settings.h"
#include "app_core_factory_reset.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *scenario;
static int wifi_context;
static app_maintenance_system_ops_t system_ops;
static app_maintenance_web_ops_t web_ops;
static app_maintenance_ota_ops_t ota_ops;
static unsigned http_stops,config_starts,activations,closes,fixed,saved,polls;
static bool config_closed,router_closed,fixed_ready;
static unsigned restart_commits,restart_cancels,result_queries;
static int64_t clock_us;
static char trace[128];static unsigned at;
static bool is(const char *name) { return !strcmp(scenario,name); }
static bool step(char name,uint32_t timeout)
{
    if (!timeout) return false; /* Admission close deliberately does not wait. */
    trace[at++]=name;
    return !(scenario[0]==name && !scenario[1]);
}
void core_boot_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
void app_camera_close_admission(void) { ++closes; }
void app_ui_close_admission(void) { assert(closes==1); }
bool app_console_uart_quiesce(uint32_t timeout) { assert(!timeout || timeout==1000);return step('U',timeout); }
esp_err_t app_input_quiesce(uint32_t timeout) { assert(!timeout || timeout==1000);return step('I',timeout)?ESP_OK:ESP_ERR_TIMEOUT; }
esp_err_t app_core_input_providers_stop(uint32_t timeout) { assert(timeout==1000);return step('P',timeout)?ESP_OK:ESP_ERR_TIMEOUT; }
bool app_ui_debug_quiesce(uint32_t timeout) { assert(!timeout || timeout==1000);return step('B',timeout); }
bool app_core_camera_quiesce(uint32_t timeout) { assert(timeout==3000);return step('C',timeout); }
bool app_core_camera_messages_quiesce(uint32_t timeout) { assert(timeout==1000);return step('E',timeout); }
esp_err_t app_core_network_quiesce(uint32_t timeout)
{ assert(timeout==3000);config_closed=step('N',timeout);return config_closed?ESP_OK:ESP_ERR_TIMEOUT; }
bool app_ui_preferences_quiesce(uint32_t timeout) { assert(timeout==1000);return step('S',timeout); }
bool app_ui_messages_quiesce(uint32_t timeout) { assert(!timeout || timeout==1000);return step('J',timeout); }
bool app_ui_renderer_quiesce(uint32_t timeout) { assert(timeout==1000);return step('D',timeout); }
bool app_core_messages_quiesce(uint32_t timeout) { assert(timeout==1000);router_closed=step('R',timeout);return router_closed; }
void app_core_messages_poll(void) { ++polls; }
esp_err_t app_ui_enter_maintenance(uint32_t timeout)
{
    assert(timeout==1000 && router_closed && config_closed && !config_starts);
    ++fixed;trace[at++]='L';fixed_ready=!is("fixed");return fixed_ready?ESP_OK:ESP_FAIL;
}
app_wifi_result_t app_wifi_config_start(app_wifi_t *wifi)
{
    assert(wifi==(app_wifi_t *)&wifi_context && router_closed && config_closed && fixed_ready);
    ++config_starts;trace[at++]='W';return is("config")?APP_WIFI_NO_MEMORY:APP_WIFI_OK;
}
app_wifi_result_t app_wifi_config_quiesce(app_wifi_t *wifi,uint32_t timeout)
{ assert(wifi==(app_wifi_t *)&wifi_context && timeout==3000);return is("config_stop")?APP_WIFI_TIMEOUT:APP_WIFI_OK; }
app_wifi_result_t app_wifi_config_result(app_wifi_t *wifi,uint32_t token,app_wifi_result_t *completed)
{ assert(wifi==(app_wifi_t *)&wifi_context && token==42);++result_queries;*completed=is("wifi_failed")?APP_WIFI_IO:APP_WIFI_OK;return is("wifi_pending")?APP_WIFI_PENDING:APP_WIFI_OK; }
esp_err_t app_maintenance_ota_init(const app_maintenance_ota_ops_t *ops)
{ ota_ops=*ops;return is("ota_init")?ESP_FAIL:ESP_OK; }
esp_err_t app_maintenance_web_init(app_wifi_t *wifi,const app_maintenance_web_ops_t *ops)
{ assert(wifi==(app_wifi_t *)&wifi_context);web_ops=*ops;return is("web_init")?ESP_FAIL:ESP_OK; }
esp_err_t app_maintenance_init(app_wifi_t *wifi,const app_maintenance_system_ops_t *ops)
{ assert(wifi==(app_wifi_t *)&wifi_context);system_ops=*ops;return is("lifecycle_init")?ESP_FAIL:ESP_OK; }
void app_maintenance_trigger_close(void) {}
esp_err_t app_maintenance_stop(void) { ++http_stops;return is("http_stop")?ESP_FAIL:ESP_OK; }
esp_err_t app_maintenance_activate(void)
{
    assert(router_closed && fixed_ready && config_starts==1 && app_core_mode_get(&app_core_mode)==APP_CORE_ACTIVATING);
    ++activations;trace[at++]='A';return is("activate")?ESP_FAIL:ESP_OK;
}
bool app_restart_prepare(void) { return true; }
void app_restart_cancel(void) { ++restart_cancels; }
bool app_restart_commit(unsigned delay) { assert(delay==1500);++restart_commits;return !is("restart_commit_failed"); }
esp_err_t app_core_settings_read(app_maintenance_settings_t *out) { *out=(app_maintenance_settings_t){1,0,1};return ESP_OK; }
esp_err_t app_core_settings_write(const app_maintenance_settings_t *settings)
{ assert(settings->version==1);++saved;return ESP_OK; }
esp_err_t app_core_factory_reset(app_wifi_t *wifi,bool all)
{ assert(wifi==(app_wifi_t *)&wifi_context && all);return ESP_OK; }
int64_t esp_timer_get_time(void) { return clock_us; }
void vTaskDelay(TickType_t ticks)
{
    clock_us+=(int64_t)ticks*1000;
    if (app_core_mode_booting(&app_core_mode)) {
        app_core_maintenance_poll();assert(!closes && !fixed && !config_starts && !at);
        if(is("boot_wait") && clock_us>=20000)app_core_mode_boot_end(&app_core_mode);
        return;
    }
    if (!is("timeout")) app_core_maintenance_poll();
}
int main(int argc,char **argv)
{
    assert(argc==2);scenario=argv[1];
    assert(app_core_maintenance_init(NULL)==ESP_ERR_INVALID_ARG);
    esp_err_t init=app_core_maintenance_init((app_wifi_t *)&wifi_context);
    if (strstr(scenario,"_init")) { assert(init==ESP_FAIL);return 0; }
    assert(init==ESP_OK && app_core_maintenance_init((app_wifi_t *)&wifi_context)==ESP_ERR_INVALID_STATE);
    assert(!web_ops.available(NULL));
    app_maintenance_settings_t settings={1,0,1};
    assert(web_ops.settings_read(NULL,&settings)==ESP_ERR_INVALID_STATE && !saved);
    assert(ota_ops.upload_begin(NULL)==ESP_ERR_INVALID_STATE);
    assert(web_ops.factory_reset(NULL,true)==ESP_ERR_INVALID_STATE);
    if (is("normal") || is("http_stop")) {
        esp_err_t error=app_core_enter_normal();
        assert(http_stops==1 && !closes && !config_starts);
        if (is("http_stop")) assert(error==ESP_FAIL && app_core_mode_get(&app_core_mode)==APP_CORE_RESTART);
        else {
            assert(error==ESP_OK && app_core_mode_get(&app_core_mode)==APP_CORE_NORMAL);
            assert(app_core_enter_normal()==ESP_OK && http_stops==2);
        }
        assert(!system_ops.request_exclusive(NULL,35000) && !closes);
        assert(!web_ops.available(NULL));return 0;
    }
    if(is("boot_wait") || is("boot_timeout"))app_core_mode_boot_begin(&app_core_mode);
    bool accepted=system_ops.request_exclusive(NULL,is("timeout") || is("boot_timeout")?20:35000);
    if(is("boot_timeout")) {
        assert(!accepted && !closes && !at && !fixed && !config_starts && !activations && clock_us==20000);
        assert(app_core_mode_get(&app_core_mode)==APP_CORE_RESTART && app_core_mode_booting(&app_core_mode));return 0;
    }
    assert(closes==1 && !http_stops); /* Never join HTTP while its callback waits. */
    if (is("success") || is("boot_wait") || is("config_stop") || is("restart_commit_failed") || !strncmp(scenario,"wifi_",5)) {
        assert(accepted && app_core_mode_get(&app_core_mode)==APP_CORE_MAINTENANCE);
        assert(!strcmp(trace,"UIPBCENSJDRLWA") && polls==1);
        assert(fixed==1 && config_starts==1 && activations==1 && web_ops.available(NULL));
        assert(!system_ops.request_exclusive(NULL,35000));
        unsigned previous=at;assert(app_core_normal_stop() && at==previous);
        assert(app_core_enter_normal()==ESP_ERR_INVALID_STATE && !http_stops);
        assert(web_ops.settings_read(NULL,&settings)==ESP_OK && settings.version==1);
        assert(ota_ops.upload_begin(NULL)==ESP_OK && ota_ops.upload_begin(NULL)==ESP_ERR_INVALID_STATE);
        assert(web_ops.settings_write(NULL,&settings)==ESP_ERR_INVALID_STATE && !saved);
        assert(web_ops.factory_reset(NULL,true)==ESP_ERR_INVALID_STATE);
        ota_ops.upload_end(NULL);assert(web_ops.factory_reset(NULL,true)==ESP_OK);assert(web_ops.settings_write(NULL,&settings)==ESP_OK && saved==1);
        if (!strncmp(scenario,"wifi_",5)) {
            web_ops.wifi_committed(NULL,42);app_core_maintenance_poll();
            assert(result_queries==1);
            assert(restart_commits==is("wifi_success") && restart_cancels==is("wifi_failed"));
            if (!is("wifi_pending")) { app_core_maintenance_poll();assert(result_queries==1); }
        } else assert(web_ops.restart_prepare(NULL) && web_ops.restart_commit(NULL,1500));
        assert(app_core_maintenance_stop(3000)==!is("config_stop") && http_stops==1);
        assert(app_core_mode_get(&app_core_mode)==APP_CORE_RESTART && ota_ops.shutting_down(NULL));
        assert(!web_ops.available(NULL) && web_ops.settings_write(NULL,&settings)==ESP_ERR_INVALID_STATE);
    } else {
        assert(!accepted && app_core_mode_get(&app_core_mode)==APP_CORE_RESTART);
        assert(!web_ops.available(NULL) && !saved);
        if (is("timeout"))assert(!fixed && !config_starts && !activations && clock_us==20000);
        else if (is("fixed"))assert(fixed==1 && !config_starts && !activations);
        else if (is("config"))assert(fixed==1 && config_starts==1 && !activations);
        else if (is("activate"))assert(fixed==1 && config_starts==1 && activations==1);
        else assert(!fixed && !config_starts && !activations);
        assert(app_core_enter_normal()==ESP_ERR_INVALID_STATE && !http_stops);
    }
    puts("Core exclusivity closes admissions, drains all owners, then activates; failures only restart");
    return 0;
}
