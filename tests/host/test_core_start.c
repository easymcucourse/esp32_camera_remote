#include "app_core.h"
#include "app_core_services.h"
#include "app_core_maintenance.h"
#include "app_core_ota_health.h"
#include "app_core_wifi_boot.h"
#include "app_camera.h"
#include "app_console.h"
#include "app_core_mode.h"
#include "esp_chip_info.h"
#include <assert.h>
#include <string.h>
#include <ctype.h>

static char trace[64], fail;
static unsigned at;
static unsigned http_cleanup,normal_cleanup;
static int wifi_context;
static esp_err_t step(char name) { trace[at++]=name;if(fail==(char)tolower(name))assert(app_core_mode_request_maintenance(&app_core_mode));return fail==name ? ESP_FAIL : ESP_OK; }
void core_boot_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error;return "fake"; }
void esp_chip_info(esp_chip_info_t *out) { step('C');out->cores=2; }
size_t esp_psram_get_size(void) { return 8*1024*1024; }
esp_err_t nvs_flash_init(void) { return step('N'); }
esp_err_t app_core_wifi_create(void) { return step('L'); }
app_wifi_t *app_core_wifi_service(void) { return (app_wifi_t *)&wifi_context; }
void app_core_wifi_config(network_config_t *out) { network_config_make_default(out); }
esp_err_t app_core_network_bind(app_wifi_t *wifi) { assert(wifi==app_core_wifi_service());return step('B'); }
esp_err_t app_ui_init(const char *ssid,const char *password)
{ assert(ssid && password);return step('I'); }
void app_ui_set_wifi_info(const char *ssid,const char *password,bool show,const char *ip,bool default_password)
{ assert(ssid && password && show && !ip && default_password);step('U'); }
bool heap_caps_check_integrity_all(bool print) { assert(print);return step('H')==ESP_OK; }
unsigned uxTaskGetStackHighWaterMark(void *task) { assert(!task);step('S');return 512; }
esp_err_t app_core_messages_start(void) { return step('M'); }
esp_err_t app_core_input_providers_start(void) { return step('P'); }
esp_err_t app_core_input_providers_prepare(void) { return step('K'); }
esp_err_t app_core_network_messages_start(void) { return step('G'); }
esp_err_t app_core_wifi_start(void) { return step('W'); }
esp_err_t app_core_maintenance_init(app_wifi_t *wifi) { assert(wifi==app_core_wifi_service());return step('E'); }
esp_err_t app_maintenance_trigger_open(void) { assert(app_core_mode_booting(&app_core_mode));esp_err_t result=step('V');if(fail=='X')assert(app_core_mode_request_maintenance(&app_core_mode));return result; }
esp_err_t app_core_camera_boot(void) { esp_err_t result=step('A');return fail=='Q'?ESP_FAIL:result; }
bool app_core_console_ready(void) { return fail!='Q' && strchr(trace,'A'); }
void app_console_freeze_subscriptions(void) { step('Z'); }
void app_ui_debug_ready(void) { step('D'); }
void app_core_ota_startup_ready(app_wifi_t *wifi) { assert(wifi==app_core_wifi_service());step('O'); }
void app_core_ota_health(void) {}
bool app_core_maintenance_stop(unsigned timeout) { assert(timeout==3000 && !app_core_mode_booting(&app_core_mode) && app_core_mode_get(&app_core_mode)==APP_CORE_RESTART);++http_cleanup;return false; }
bool app_core_normal_stop(void) { assert(http_cleanup==1 && !app_core_mode_booting(&app_core_mode));++normal_cleanup;return false; }
bool app_core_camera_quiesce(uint32_t timeout) { (void)timeout;return true; }
esp_err_t app_core_health_start(const app_core_health_ops_t *ops)
{ assert(ops->health_tick==app_core_ota_health && ops->maintenance_quiesce==app_core_maintenance_stop && ops->camera_drain==app_core_camera_quiesce);return step('T'); }
int main(int argc,char **argv)
{
    assert(argc==2);fail=argv[1][0];
    const char *normal=
#if CONFIG_REMOTE_DBG_SIM
        "CNLB IUHSKWEVMGPAZDOT";
#else
        "CNLB IUHSKWEVMGPAZOT";
#endif
    char expected[64]={0};unsigned size=0;
    bool claimed=false,camera=false;
    for(unsigned i=0;normal[i];++i) {
        char name=normal[i];
        if(name==' ' || (strchr("MGPA",name) && claimed) || (name=='Z' && (fail=='Q' || !camera)))continue;
        expected[size++]=name;if(name=='A')camera=true;
        if((name=='V' && fail=='X') || fail==(char)tolower(name))claimed=true;
    }
    esp_err_t result=app_core_start();
    bool fatal=fail!='0' && fail!='N' && fail!='X' && !strchr("mgpa",fail);
    if(fatal) {
        assert(result==ESP_FAIL);
        char *last=strchr(expected,fail=='Q'?'A':fail);assert(last);last[1]=0;
    } else assert(result==ESP_OK);
    assert(!strcmp(trace,expected));
    assert(!app_core_mode_booting(&app_core_mode));
    assert(http_cleanup==fatal && normal_cleanup==fatal);
    assert(app_core_mode_get(&app_core_mode)==(fatal?APP_CORE_RESTART:claimed?APP_CORE_ACTIVATING:APP_CORE_STARTUP));
    assert(app_core_start()==ESP_ERR_INVALID_STATE && !strcmp(trace,expected));
    return 0;
}
