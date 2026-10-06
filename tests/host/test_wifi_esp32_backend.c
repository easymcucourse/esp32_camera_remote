/* Real ESP32 factory and facade; SDK, storage, TCP and jobs are boundaries. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "../../components/wifi_esp32/wifi_esp32.c"
static unsigned step,fail_step,locks,jobs,driver,registrations,loop_created,loop_deleted;
static unsigned mutex_calls,fail_mutex;
static bool existing_loop,fail_jobs,job_start_fail,radio_started;
static app_wifi_result_t job_stop_result=APP_WIFI_OK;
static esp_err_t stop_result=ESP_OK;
static esp_netif_t netif;
static esp_event_handler_t handler;
static void *handler_context;
static network_config_t saved,active_config;
static wifi_sta_list_t peers;
static bool leased[ESP_WIFI_MAX_CONN_NUM];
static wifi_config_t radio_config;
struct fake_semaphore { int taken; };
struct wifi_config_jobs { void *context;wifi_job_io_t save,restart; };
static esp_err_t sdk(void) { return ++step==fail_step?ESP_FAIL:ESP_OK; }
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{ if(++mutex_calls==fail_mutex)return NULL;SemaphoreHandle_t p=calloc(1,sizeof(*p));assert(p);++locks;return p; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem,TickType_t timeout)
{ (void)timeout;SemaphoreHandle_t p=sem;assert(p);if(p->taken)return pdFALSE;p->taken=1;return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{ SemaphoreHandle_t p=sem;assert(p && p->taken);p->taken=0;return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t sem) { assert(sem && !sem->taken && locks);--locks;free(sem); }
esp_err_t esp_netif_init(void) { return sdk(); }
esp_err_t esp_event_loop_create_default(void)
{ esp_err_t r=sdk();if(r!=ESP_OK)return r;if(existing_loop)return ESP_ERR_INVALID_STATE;++loop_created;return ESP_OK; }
esp_err_t esp_event_loop_delete_default(void) { ++loop_deleted;assert(loop_deleted<=loop_created);return ESP_OK; }
esp_netif_t *esp_netif_create_default_wifi_ap(void)
{ if(sdk()!=ESP_OK)return NULL;assert(!netif.alive);netif.alive=1;return &netif; }
void esp_netif_destroy_default_wifi(esp_netif_t *p) { assert(p==&netif && p->alive);p->alive=0; }
bool esp_netif_is_netif_up(esp_netif_t *p) { assert(p==&netif);return radio_started; }
esp_err_t esp_netif_get_ip_info(esp_netif_t *p,esp_netif_ip_info_t *out)
{ assert(p==&netif);out->ip.addr=0x0104a8c0;return ESP_OK; }
esp_err_t esp_netif_dhcps_get_clients_by_mac(esp_netif_t *p,unsigned n,esp_netif_pair_mac_ip_t *out)
{ assert(p==&netif && n==peers.num);for(unsigned i=0;i<n;++i){assert(!memcmp(out[i].mac,peers.sta[i].mac,6));out[i].ip.addr=leased[i]?0x0204a8c0:0;}return ESP_OK; }
esp_err_t esp_wifi_init(const wifi_init_config_t *c)
{ assert(!c->nvs_enable && !driver);esp_err_t r=sdk();if(r==ESP_OK)driver=1;return r; }
esp_err_t esp_wifi_deinit(void) { assert(driver && !radio_started);driver=0;return ESP_OK; }
esp_err_t esp_wifi_set_storage(unsigned value) { assert(value==WIFI_STORAGE_RAM);return sdk(); }
esp_err_t esp_wifi_set_country_code(const char *s,bool ieee) { assert(!strcmp(s,"JP") && ieee);return sdk(); }
esp_err_t esp_wifi_get_country(wifi_country_t *out) { *out=(wifi_country_t){1,13};return sdk(); }
esp_err_t esp_event_handler_instance_register(esp_event_base_t b,int32_t id,esp_event_handler_t fn,void *c,esp_event_handler_instance_t *out)
{ assert(!strcmp(b,WIFI_EVENT) && id==ESP_EVENT_ANY_ID && !registrations);esp_err_t r=sdk();if(r==ESP_OK){handler=fn;handler_context=c;*out=(void *)1;registrations=1;}return r; }
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t b,int32_t id,esp_event_handler_instance_t h)
{ assert(!strcmp(b,WIFI_EVENT) && id==ESP_EVENT_ANY_ID && h==(void *)1 && registrations);registrations=0;handler=NULL;return ESP_OK; }
esp_err_t esp_wifi_set_mode(unsigned m) { assert(m==WIFI_MODE_AP);return sdk(); }
esp_err_t esp_wifi_set_config(unsigned iface,const wifi_config_t *c)
{ assert(iface==WIFI_IF_AP && c->ap.authmode==WIFI_AUTH_WPA2_PSK && !c->ap.pmf_cfg.required);radio_config=*c;return ESP_OK; }
esp_err_t esp_wifi_start(void) { assert(driver && handler && !radio_started);radio_started=true;handler(handler_context,WIFI_EVENT,WIFI_EVENT_AP_START,NULL);return ESP_OK; }
esp_err_t esp_wifi_stop(void)
{ if(stop_result!=ESP_OK)return stop_result;radio_started=false;if(handler)handler(handler_context,WIFI_EVENT,WIFI_EVENT_AP_STOP,NULL);return ESP_OK; }
esp_err_t esp_wifi_ap_get_sta_list(wifi_sta_list_t *out) { *out=peers;return ESP_OK; }
esp_err_t wifi_saved_read(network_config_t *out) {*out=saved;return ESP_OK; }
esp_err_t wifi_saved_write(const network_config_t *value) { saved=*value;return ESP_OK; }
wifi_config_jobs_t *wifi_jobs_create(void *c,wifi_job_io_t save,wifi_job_io_t restart)
{ if(fail_jobs)return NULL;wifi_config_jobs_t *p=malloc(sizeof(*p));assert(p);*p=(wifi_config_jobs_t){c,save,restart};++jobs;return p; }
void wifi_jobs_destroy(wifi_config_jobs_t *p) { if(p){assert(jobs);--jobs;free(p);} }
void wifi_jobs_set(wifi_config_jobs_t *p,const network_config_t *c,unsigned maximum) { assert(p && maximum==13);active_config=*c; }
app_wifi_result_t wifi_jobs_start(wifi_config_jobs_t *p) { assert(p);return job_start_fail?APP_WIFI_NO_MEMORY:APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_stop(wifi_config_jobs_t *p,uint32_t timeout) { assert(p);(void)timeout;return job_stop_result; }
app_wifi_result_t wifi_jobs_get(wifi_config_jobs_t *p,network_config_t *out) { assert(p);*out=active_config;return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_apply(wifi_config_jobs_t *p,const network_config_t *c,bool staged,uint32_t *token)
{ assert(p && staged);(void)c;*token=7;return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_commit(wifi_config_jobs_t *p,uint32_t token,unsigned delay) { assert(p && token==7 && delay==1500);return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_cancel(wifi_config_jobs_t *p,uint32_t token) { assert(p && token==7);return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_result(wifi_config_jobs_t *p,uint32_t token,app_wifi_result_t *out) { assert(p && token==7);*out=APP_WIFI_OK;return APP_WIFI_OK; }
app_wifi_result_t wifi_jobs_freeze(wifi_config_jobs_t *p,uint32_t timeout) { assert(p);(void)timeout;return APP_WIFI_OK; }
void wifi_jobs_resume(wifi_config_jobs_t *p) { assert(p); }
app_wifi_result_t wifi_tcp_open(void *c,wifi_tcp_network_fn n,wifi_tcp_diagnostic_fn d,const app_wifi_endpoint_t *e,const app_wifi_deadline_t *t,void **out)
{ (void)c;(void)n;(void)d;(void)e;(void)t;(void)out;return APP_WIFI_UNSUPPORTED; }
app_wifi_result_t wifi_tcp_io(void *c,void *data,size_t size,bool tx,const app_wifi_deadline_t *t,size_t *bytes)
{ (void)c;(void)data;(void)size;(void)tx;(void)t;(void)bytes;return APP_WIFI_UNSUPPORTED; }
void wifi_tcp_close(void *c) { (void)c;assert(false); }
app_wifi_result_t wifi_tcp_poll(void *c,const app_wifi_deadline_t *t,bool *readable)
{ (void)c;(void)t;(void)readable;return APP_WIFI_UNSUPPORTED; }
static void resources_gone(void)
{ assert(!locks && !jobs && !driver && !registrations && !netif.alive && !radio_started && loop_created==loop_deleted); }
int main(void)
{
    network_config_make_default(&saved);
    app_wifi_t *wifi=NULL,*other=NULL;
    for(unsigned failure=1;failure<=2;++failure){mutex_calls=0;fail_mutex=failure;assert(wifi_esp32_create(&wifi)==APP_WIFI_NO_MEMORY && !wifi);resources_gone();}
    fail_mutex=0;fail_jobs=true;assert(wifi_esp32_create(&wifi)==APP_WIFI_NO_MEMORY && !wifi);resources_gone();fail_jobs=false;
    for(unsigned failure=1;failure<=9;++failure){
        assert(wifi_esp32_create(&wifi)==APP_WIFI_OK);step=0;fail_step=failure;
        assert(app_wifi_init(wifi)==(failure==3?APP_WIFI_NO_MEMORY:APP_WIFI_IO));
        assert(!driver && !registrations && !netif.alive);
        assert(app_wifi_destroy(&wifi)==APP_WIFI_OK && !wifi);resources_gone();
    }
    fail_step=0;step=0;existing_loop=true;
    assert(wifi_esp32_create(&wifi)==APP_WIFI_OK && locks==2 && jobs==1 && !driver);
    assert(wifi_esp32_create(&other)==APP_WIFI_STATE && !other);
    network_config_t value;assert(app_wifi_saved_config_read(wifi,&value)==APP_WIFI_OK && !strcmp(value.ssid,saved.ssid));
    assert(app_wifi_init(wifi)==APP_WIFI_OK && app_wifi_init(wifi)==APP_WIFI_STATE);
    wifi_esp32_t *backend=handler_context;
    assert(xSemaphoreTake(backend->storage,0)==pdTRUE);
    assert(app_wifi_saved_config_read(wifi,&value)==APP_WIFI_STATE);
    assert(app_wifi_saved_config_write(wifi,&saved)==APP_WIFI_STATE);
    assert(xSemaphoreGive(backend->storage)==pdTRUE);
    assert(app_wifi_saved_config_write(wifi,&saved)==APP_WIFI_OK);
    app_wifi_status_t snapshot;assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && !snapshot.started && snapshot.max_channel==13);
    assert(xSemaphoreTake(backend->lifecycle,0)==pdTRUE);
    assert(app_wifi_start(wifi,&saved)==APP_WIFI_STATE && !radio_started);
    assert(xSemaphoreGive(backend->lifecycle)==pdTRUE);
    job_start_fail=true;assert(app_wifi_start(wifi,&saved)==APP_WIFI_NO_MEMORY && !radio_started);
    job_start_fail=false;assert(app_wifi_start(wifi,&saved)==APP_WIFI_OK && radio_started);
    assert(radio_config.ap.channel==6 && radio_config.ap.ssid_len==strlen(saved.ssid) && radio_config.ap.max_connection==APP_WIFI_CLIENT_CAPACITY);
    assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && snapshot.started && snapshot.online && !strcmp(snapshot.address,"192.168.4.1"));
    uint32_t generation=snapshot.generation;
    assert(app_wifi_reconfigure(wifi,&saved)==APP_WIFI_OK && radio_started);
    assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && snapshot.generation!=generation);
    generation=snapshot.generation;
    handler(handler_context,WIFI_EVENT,99,NULL);assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && snapshot.online);
    peers.num=2;peers.sta[0].mac[0]=1;peers.sta[0].rssi=-45;peers.sta[1].mac[0]=2;leased[1]=true;
    app_wifi_client_t found[2];size_t count=0;
    assert(app_wifi_get_clients(wifi,found,2,&count)==APP_WIFI_OK && count==1 && found[0].mac[0]==2);
    leased[0]=true;assert(app_wifi_get_clients(wifi,found,1,&count)==APP_WIFI_NO_MEMORY && !count);
    job_stop_result=APP_WIFI_TIMEOUT;assert(app_wifi_stop(wifi,20)==APP_WIFI_TIMEOUT && radio_started);
    assert(app_wifi_destroy(&wifi)==APP_WIFI_STATE && wifi && locks==2);
    job_stop_result=APP_WIFI_OK;stop_result=ESP_ERR_TIMEOUT;assert(app_wifi_stop(wifi,20)==APP_WIFI_TIMEOUT && radio_started);
    stop_result=ESP_OK;assert(app_wifi_stop(wifi,20)==APP_WIFI_OK && !radio_started);
    assert(app_wifi_get_status(wifi,&snapshot)==APP_WIFI_OK && !snapshot.started && !snapshot.online && snapshot.generation!=generation);
    assert(app_wifi_destroy(&wifi)==APP_WIFI_OK && !wifi);resources_gone();
    existing_loop=false;assert(wifi_esp32_create(&wifi)==APP_WIFI_OK && app_wifi_init(wifi)==APP_WIFI_OK);
    assert(app_wifi_destroy(&wifi)==APP_WIFI_OK);resources_gone();
    return 0;
}
