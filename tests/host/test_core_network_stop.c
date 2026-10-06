#include "app_core.h"
#include "app_core_services.h"
#include "app_core_network.h"
#include "app_wifi_driver.h"
#include <assert.h>
#include <string.h>

static int64_t now;
static unsigned bridge_calls, radio_stops, radio_changes, config_calls, saved_writes;
static uint32_t bridge_budget;
static bool closed;
static unsigned config_starts;
static app_wifi_result_t config_start_result=APP_WIFI_NO_MEMORY;
static app_wifi_result_t config_stop_result=APP_WIFI_TIMEOUT;
static esp_err_t bridge_result=ESP_ERR_TIMEOUT;
static network_config_t config;
esp_err_t app_wifi_messages_start(app_wifi_t *wifi) { assert(wifi);return ESP_OK; }
int64_t esp_timer_get_time(void) { return now; }
esp_err_t app_wifi_messages_stop(uint32_t timeout)
{ ++bridge_calls;bridge_budget=timeout;return bridge_result; }
static app_wifi_result_t start(void *context,const network_config_t *value)
{ assert(context==&config);config=*value;return APP_WIFI_OK; }
static app_wifi_result_t reconfigure(void *context,const network_config_t *value)
{ (void)context;(void)value;++radio_changes;return APP_WIFI_OK; }
static app_wifi_result_t stop(void *context,uint32_t timeout)
{ (void)context;(void)timeout;++radio_stops;return APP_WIFI_OK; }
static app_wifi_result_t status(void *context,app_wifi_status_t *out)
{ assert(context==&config);*out=(app_wifi_status_t){.online=true,.generation=42,.max_channel=13};return APP_WIFI_OK; }
static app_wifi_result_t clients(void *context,app_wifi_client_t *out,size_t capacity,size_t *count)
{ (void)context;(void)out;(void)capacity;*count=0;return APP_WIFI_OK; }
static void destroy(void *context) { (void)context; }
static app_wifi_result_t get(void *context,network_config_t *out)
{ assert(context==&config);*out=config;return APP_WIFI_OK; }
static app_wifi_result_t saved_write(void *context,const network_config_t *value)
{ assert(context==&config && value);++saved_writes;return APP_WIFI_OK; }
static app_wifi_result_t apply(void *context,const network_config_t *value,bool staged,uint32_t *token)
{ (void)context;(void)value;(void)staged;(void)token;assert(closed);return APP_WIFI_STATE; }
static app_wifi_result_t commit(void *context,uint32_t token,unsigned delay)
{ (void)context;(void)token;(void)delay;assert(closed);return APP_WIFI_STATE; }
static app_wifi_result_t cancel(void *context,uint32_t token)
{ (void)context;(void)token;return APP_WIFI_OK; }
static app_wifi_result_t result(void *context,uint32_t token,app_wifi_result_t *out)
{ (void)context;assert(token==7);*out=APP_WIFI_CANCELLED;return APP_WIFI_OK; }
static app_wifi_result_t freeze(void *context,uint32_t timeout)
{ (void)context;(void)timeout;return APP_WIFI_OK; }
static void resume(void *context) { (void)context; }
static app_wifi_result_t start_config(void *context)
{ assert(context==&config && closed);++config_starts;if(config_start_result==APP_WIFI_OK)closed=false;return config_start_result; }
static app_wifi_result_t quiesce(void *context,uint32_t timeout)
{ assert(context==&config && timeout==100);closed=true;++config_calls;now+=40000;return config_stop_result; }
static app_wifi_result_t open_channel(void *context,const app_wifi_endpoint_t *endpoint,
    const app_wifi_deadline_t *deadline,void **out)
{ (void)context;(void)endpoint;(void)deadline;(void)out;return APP_WIFI_UNSUPPORTED; }
static app_wifi_result_t io(void *context,void *data,size_t size,bool tx,const app_wifi_deadline_t *deadline,size_t *bytes)
{ (void)context;(void)data;(void)size;(void)tx;(void)deadline;(void)bytes;return APP_WIFI_UNSUPPORTED; }
static void close_channel(void *context) { (void)context; }
int main(void)
{
    network_config_make_default(&config);
    app_wifi_driver_ops_t ops={.api_version=APP_WIFI_API_VERSION,
        .capabilities=APP_WIFI_CAP_AP|APP_WIFI_CAP_CONFIG_STORE|APP_WIFI_CAP_CONFIG_ASYNC|APP_WIFI_CAP_TCP,
        .start=start,.reconfigure=reconfigure,.stop=stop,.status=status,.clients=clients,.destroy=destroy,
        .saved_read=get,.saved_write=saved_write,.config_get=get,.config_apply=apply,.config_commit=commit,
        .config_cancel=cancel,.config_result=result,.config_freeze=freeze,.config_resume=resume,
        .channel_open=open_channel,.channel_io=io,.channel_close=close_channel};
    app_wifi_t *wifi=NULL;
    assert(app_core_network_quiesce(100)==ESP_ERR_INVALID_STATE);
    assert(app_core_network_bind(NULL)==ESP_ERR_INVALID_ARG);
    assert(app_wifi_config_quiesce(NULL,0)==APP_WIFI_INVALID);
    assert(app_wifi_driver_bind(&ops,&config,&wifi)==APP_WIFI_OK);
    --ops.api_version;assert(app_core_network_bind(wifi)==ESP_ERR_INVALID_ARG);++ops.api_version;
    ops.capabilities=APP_WIFI_CAP_AP;
    assert(app_core_network_bind(wifi)==ESP_ERR_INVALID_ARG);
    assert(app_wifi_config_quiesce(wifi,0)==APP_WIFI_UNSUPPORTED);
    ops.capabilities=APP_WIFI_CAP_AP|APP_WIFI_CAP_CONFIG_STORE|APP_WIFI_CAP_CONFIG_ASYNC|APP_WIFI_CAP_TCP;
    assert(app_wifi_start(wifi,&config)==APP_WIFI_OK);
    assert(app_core_network_bind(wifi)==ESP_OK);
    assert(app_core_network_bind(wifi)==ESP_ERR_INVALID_STATE);
    assert(app_core_network_quiesce(100)==ESP_ERR_NOT_SUPPORTED && !bridge_calls);
    ops.config_quiesce=quiesce;
    assert(app_core_network_quiesce(100)==ESP_ERR_TIMEOUT && config_calls==1 && !bridge_calls);
    uint32_t token=0;
    assert(app_wifi_config_apply(wifi,&config,true,&token)==APP_WIFI_STATE && !token);
    assert(app_wifi_config_commit(wifi,7,0)==APP_WIFI_STATE);
    config_stop_result=APP_WIFI_OK;
    assert(app_core_network_quiesce(100)==ESP_ERR_TIMEOUT && bridge_calls==1 && bridge_budget==60);
    bridge_result=ESP_OK;
    assert(app_core_network_quiesce(100)==ESP_OK && bridge_calls==2 && bridge_budget==60);
    assert(app_core_network_quiesce(100)==ESP_OK && bridge_calls==3);
    app_wifi_status_t snapshot;
    assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && snapshot.started && snapshot.online && snapshot.generation==42);
    app_wifi_result_t completed;
    assert(app_wifi_config_result(wifi,7,&completed)==APP_WIFI_OK && completed==APP_WIFI_CANCELLED);
    network_config_t copied;
    assert(app_wifi_config_get(wifi,&copied)==APP_WIFI_OK && !strcmp(copied.ssid,config.ssid));
    assert(app_wifi_saved_config_write(wifi,&copied)==APP_WIFI_OK && saved_writes==1);
    assert(!radio_stops && !radio_changes);
    return 0; /* Bound object intentionally remains owned for maintenance. */
    assert(app_wifi_config_start(NULL)==APP_WIFI_INVALID);
    assert(app_wifi_config_start(wifi)==APP_WIFI_UNSUPPORTED);
    ops.config_start=start_config;
    assert(app_wifi_config_start(wifi)==APP_WIFI_NO_MEMORY && closed && config_starts==1);
    config_start_result=APP_WIFI_OK;
    assert(app_wifi_config_start(wifi)==APP_WIFI_OK && !closed && config_starts==2);
    assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && snapshot.started && snapshot.online && snapshot.generation==42);
    assert(!radio_stops && !radio_changes);

}
