#include <assert.h>
#include <setjmp.h>
#include <string.h>
unsigned uxTaskGetStackHighWaterMark(void *task);
#include "../../components/app_ui/ui_message_endpoint.c"
static TaskFunction_t task;
static jmp_buf idle;
static bool active,run_on_wait,in_task,create_fail,stop_in_render,frozen;
static bool cleanup_ready=true;
static unsigned subscriptions,cleanups;
static unsigned pressure,renders,drops,leases,results,retired,replies;
static int64_t now;
static app_message_t inbox[4];static unsigned used;
static void run(void) { assert(task && !in_task);in_task=true;if(!setjmp(idle))task(NULL);in_task=false; }
void vTaskDelay(TickType_t ticks) { now+=(int64_t)ticks*1000;if(run_on_wait && !in_task && task)run(); }
int64_t esp_timer_get_time(void) { return now; }
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
unsigned uxTaskGetStackHighWaterMark(void *t) { (void)t;return 4096; }
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn,const char *name,unsigned stack,void *ctx,unsigned priority,void *handle,unsigned core,unsigned caps)
{ assert(!strcmp(name,"ui_endpoint") && stack==32768 && priority==4 && core==1 && !ctx && !handle && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));if(create_fail)return pdFALSE;task=fn;return pdPASS; }
void vTaskDeleteWithCaps(void *t) { assert(!t && in_task);task=NULL;longjmp(idle,1); }
esp_err_t app_console_endpoint_register(app_endpoint_t endpoint,const app_endpoint_config_t *config)
{ assert(endpoint==APP_ENDPOINT_UI && config->control_depth==16 && config->bulk_depth==2);if(active)return ESP_ERR_INVALID_STATE;active=true;return ESP_OK; }
void app_console_endpoint_stop(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_UI);active=false;++retired; }
void app_console_get_status(app_console_status_t *s) { *s=(app_console_status_t){.running=true,.accepting=true,.subscriptions_frozen=frozen}; }
esp_err_t app_console_subscribe(app_message_type_t type,app_endpoint_t endpoint)
{ assert(endpoint==APP_ENDPOINT_UI && active);(void)type;++subscriptions;return frozen?ESP_ERR_INVALID_STATE:ESP_OK; }
esp_err_t app_console_receive(app_endpoint_t endpoint,app_message_t *message,uint32_t timeout)
{ assert(endpoint==APP_ENDPOINT_UI);assert(timeout==(atomic_load(&closing)?0:250));if(!active)return ESP_ERR_INVALID_STATE;if(used){*message=inbox[0];memmove(inbox,inbox+1,--used*sizeof(*inbox));return ESP_OK;}if(!atomic_load(&closing))longjmp(idle,1);return ESP_ERR_TIMEOUT; }
esp_err_t app_console_reply(const app_message_t *request,app_message_t *reply)
{ assert(request->flags&APP_MESSAGE_REQUEST);assert(reply->result==ESP_ERR_INVALID_STATE);++replies;return ESP_OK; }
esp_err_t app_console_send(app_message_t *m)
{ assert(m->type==APP_MESSAGE_UI_FRAME_RESULT && !m->lease && m->payload.command.token);if(!active)return ESP_ERR_INVALID_STATE;if(pressure){--pressure;return ESP_ERR_TIMEOUT;}++results;if(m->result==ESP_ERR_INVALID_STATE)++drops;return ESP_OK; }
void app_message_release(app_message_t *m) { if(m->lease){++leases;m->lease=NULL;} }
const void *app_message_lease_data(const app_message_lease_t *lease,size_t *size)
{ static unsigned char jpeg[8];assert(lease);*size=8;return jpeg; }
void *app_message_lease_write(app_message_lease_t *lease,size_t *size) { (void)lease;(void)size;return NULL; }
bool ui_camera_generation_accept(uint32_t generation) { return generation==7; }
esp_err_t app_ui_show_jpeg(const uint8_t *jpeg,size_t size)
{ assert(jpeg && size==8);++renders;if(stop_in_render){stop_in_render=false;assert(!app_ui_messages_quiesce(0) && active);}return ESP_OK; }
esp_err_t app_ui_recover_display(void) { return ESP_OK; }
esp_err_t ui_preferences_start(void) { return ESP_OK; }
bool ui_preferences_quiesce(uint32_t t) { (void)t;++cleanups;return cleanup_ready; }
esp_err_t ui_wifi_menu_start(void) { return ESP_OK; }
bool ui_wifi_menu_quiesce(uint32_t t) { (void)t;++cleanups;return cleanup_ready; }
esp_err_t ui_preferences_message(const app_message_t *m,app_message_t *r,bool *d) { (void)m;(void)r;(void)d;assert(false);return ESP_FAIL; }
esp_err_t ui_menu_message_apply(const app_message_t *m,app_message_t *r) { (void)m;(void)r;return ESP_OK; }
esp_err_t ui_property_message(const app_message_t *m,app_message_t *r) { (void)m;(void)r;return ESP_OK; }
void ui_menu_snapshot(app_ui_state_t *state) { memset(state,0,sizeof(*state)); }
esp_err_t ui_input_message_apply(const app_message_t *m) { (void)m;return ESP_OK; }
esp_err_t ui_camera_message_apply(const app_message_t *m) { (void)m;return ESP_OK; }
void app_ui_set_wifi_rssi(int value) { (void)value; }
void app_ui_set_wifi_info(const char *ssid,const char *password,bool show,const char *ip,bool def) { (void)ssid;(void)password;(void)show;(void)ip;(void)def; }
void app_ui_refresh_wifi_info(void) {}
static void frame(unsigned token)
{ assert(used<4);inbox[used++]=(app_message_t){.type=APP_MESSAGE_CAMERA_FRAME,.source=APP_ENDPOINT_CAMERA,.generation=7,.lease=(app_message_lease_t*)1,.payload.command={.token=token}}; }
int main(void)
{
 create_fail=true;assert(app_ui_messages_start()==ESP_ERR_NO_MEM && !active && !atomic_load(&started));
 create_fail=false;assert(app_ui_messages_start()==ESP_OK && active);frozen=true;unsigned saved_subscriptions=subscriptions;
 assert(app_ui_messages_start()==ESP_ERR_INVALID_STATE);
 frame(1);frame(2);inbox[used++]=(app_message_t){.type=APP_MESSAGE_UI_PREFERENCES,.flags=APP_MESSAGE_REQUEST};
 pressure=3;assert(!app_ui_messages_quiesce(0) && active && task);
 run_on_wait=true;assert(app_ui_messages_quiesce(1000) && !active && !task && !ui_frames_pending());
 assert(leases==2 && results==2 && drops==2 && !renders && replies==1);
 assert(app_ui_messages_quiesce(0));
 assert(app_ui_messages_start()==ESP_OK);frame(3);frame(4);stop_in_render=true;run();
 assert(app_ui_messages_quiesce(0) && leases==4 && renders==1 && results==4 && drops==3 && !active);
 assert(retired==3 && cleanups==1 && subscriptions==saved_subscriptions);
 assert(app_ui_messages_start()==ESP_OK);frame(5);pressure=100;run();
 assert(ui_frames_pending() && leases==5);
 run_on_wait=false;assert(!app_ui_messages_quiesce(10) && active && task && ui_frames_pending());
 pressure=0;run_on_wait=true;assert(app_ui_messages_quiesce(1000) && !ui_frames_pending() && results==5);
 assert(retired==4);
 create_fail=true;cleanup_ready=false;
 assert(app_ui_messages_start()==ESP_ERR_NO_MEM && !active && !atomic_load(&started));
 create_fail=false;assert(app_ui_messages_start()==ESP_ERR_INVALID_STATE && !active);
 cleanup_ready=true;assert(app_ui_messages_start()==ESP_OK && active);
 assert(app_ui_messages_quiesce(1000) && !active && !task && retired==6);
 assert(app_ui_messages_start()==ESP_OK && active);unsigned previous_renders=renders;
 app_ui_close_admission();frame(6);
 assert(atomic_load(&admission_closed) && !atomic_load(&closing) && app_ui_messages_start()==ESP_ERR_INVALID_STATE);
 run();assert(active && task && !atomic_load(&closing) && !ui_frames_pending());
 assert(leases==6 && results==6 && renders==previous_renders);
 assert(app_ui_messages_quiesce(1000) && !active && !task && leases==6 && renders==previous_renders && results==6);
 assert(app_ui_messages_start()==ESP_ERR_INVALID_STATE); /* Irreversible exclusive closure. */
 return 0;
}

esp_err_t ui_mode_enter_normal(unsigned reason) { assert(reason<=APP_NORMAL_DISPLAY_TEST);return ESP_OK; }
