#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "../../components/app_ui/ui_renderer.c"
struct fake_semaphore { bool taken; };
static uint16_t buffers[2][UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT];
static display_prepare_t prepare;
static display_canvas_t *writer;
static bool ready=true,publish_fail,release_user,run_refresh;
static TaskFunction_t task;
static jmp_buf done;
static int64_t now;
static unsigned front,publishes,notifications,resets,recoveries,faults,draws;
static char last_text[96];
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return calloc(1,sizeof(struct fake_semaphore)); }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t timeout) { (void)timeout;if(s->taken)return pdFALSE;s->taken=true;return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(s->taken);s->taken=false;return pdTRUE; }
void fake_log(const char *tag,const char *format,...) { (void)tag;(void)format; }
const char *esp_err_to_name(esp_err_t e) { (void)e;return "fake"; }
int64_t esp_timer_get_time(void) { return now; }
void vTaskDelay(TickType_t ticks)
{ now+=(int64_t)ticks*1000;if(release_user){release_user=false;ui_render_leave();}if(run_refresh && task){TaskFunction_t fn=task;task=NULL;if(!setjmp(done))fn(NULL);} }
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait) { assert(clear && wait==250);return 0; }
void xTaskNotifyGive(TaskHandle_t t) { assert(t);++notifications; }
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn,const char *name,unsigned stack,void *ctx,unsigned priority,void *handle,unsigned core,unsigned caps)
{ assert(!strcmp(name,"lcd_status") && stack==32768 && priority==2 && core==1 && caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT) && !ctx);task=fn;*(TaskHandle_t*)handle=(void*)1;return pdPASS; }
void vTaskDeleteWithCaps(void *t) { assert(!t);longjmp(done,1); }
esp_err_t ui_fonts_init(void) { return ESP_OK; }
int ui_fonts_measure(const char *text,int size,bool numbers) { (void)numbers;return (int)strlen(text)*size/2; }
int ui_fonts_line_height(int size) { return size; }
void ui_fonts_draw(uint16_t *pixels,int width,int height,int left,int top,const char *text,int size,uint16_t color,bool numbers,int clip)
{ (void)size;(void)numbers;(void)clip;assert(width==UI_CANVAS_WIDTH && height==UI_CANVAS_HEIGHT);strncpy(last_text,text,sizeof(last_text)-1);++draws;if(maintenance_published)assert(!strcmp(text,"MAINTENANCE"));if(left>=0 && left<width && top>=0 && top<height)pixels[top*width+left]=color; }
esp_err_t display_surface_init(display_prepare_t fn,esp_err_t (*fonts)(void)) { assert(fonts()==ESP_OK);prepare=fn;prepare(buffers[0]);prepare(buffers[1]);return ESP_OK; }
void display_surface_get_status(display_surface_status_t *s) { *s=(display_surface_status_t){.ready=ready,.width=UI_CANVAS_WIDTH,.height=UI_CANVAS_HEIGHT,.stride_pixels=UI_CANVAS_WIDTH}; }
esp_err_t display_surface_recover(void) { assert(!writer);++recoveries;ready=true;prepare(buffers[0]);prepare(buffers[1]);return ESP_OK; }
esp_err_t display_surface_test_fault(unsigned mode) { (void)mode;++faults;return ESP_OK; }
esp_err_t display_canvas_acquire(display_canvas_t *c,uint32_t timeout)
{ (void)timeout;if(writer || !ready)return ESP_ERR_INVALID_STATE;writer=c;*c=(display_canvas_t){.pixels=buffers[1-front],.width=UI_CANVAS_WIDTH,.height=UI_CANVAS_HEIGHT,.stride_pixels=UI_CANVAS_WIDTH,.lease=1};return ESP_OK; }
esp_err_t display_canvas_refresh(display_canvas_t *c)
{ assert(c==writer && c->lease);writer=NULL;c->lease=0;++publishes;if(publish_fail){ready=false;return ESP_FAIL;}front=1-front;return ESP_OK; }
void display_canvas_cancel(display_canvas_t *c) { if(c->lease){assert(c==writer);writer=NULL;c->lease=0;} }
esp_err_t ui_jpeg_init(void) { return ESP_OK; }
void ui_jpeg_reset(void) { assert(display_mutex->taken && ui_render_idle());++resets; }
void ui_jpeg_reset_fps(void) {}
int main(void)
{
 assert(app_ui_init("ssid","masked")==ESP_OK && task && refresh_task);
 assert(app_ui_show_connection("ready")==ESP_OK && publishes==1);
 app_ui_refresh_wifi_info();unsigned notified=notifications;
 assert(ui_render_enter());assert(!app_ui_renderer_quiesce(0) && task && resets==0);
 assert(ui_render_stopping() && !ui_render_enter());
 assert(app_ui_show_connection("late")==ESP_ERR_INVALID_STATE);
 assert(app_ui_recover_display()==ESP_ERR_INVALID_STATE && !recoveries);
 assert(app_ui_test_display_fault(1)==ESP_ERR_INVALID_STATE && !faults);
 app_ui_refresh_wifi_info();assert(notifications==notified);
 release_user=run_refresh=true;assert(app_ui_renderer_quiesce(100) && !task && !refresh_task && resets==1);
 publish_fail=true;assert(app_ui_enter_maintenance(100)==ESP_ERR_INVALID_STATE && !maintenance_published && !writer);
 assert(atomic_load(&ui_model_frozen) && !ui_model_connection_ssid[0]);
 assert(app_ui_show_connection("still closed")==ESP_ERR_INVALID_STATE);
 publish_fail=false;assert(app_ui_enter_maintenance(100)==ESP_OK && maintenance_published && !strcmp(last_text,"MAINTENANCE"));
 assert(recoveries==1 && !writer);unsigned lit=0;
 for(unsigned i=0;i<UI_CANVAS_WIDTH*UI_CANVAS_HEIGHT;++i)lit+=buffers[front][i]!=0;
 assert(lit==1);unsigned count=publishes,draw_count=draws;
 app_ui_set_wifi_info("new","new",true,"1.2.3.4",false);app_ui_refresh_wifi_info();
 assert(!ui_model_connection_ssid[0] && !ui_model_connection_password[0]);
 assert(app_ui_enter_maintenance(0)==ESP_OK && publishes==count && draws==draw_count && notifications==notified);
 assert(!ui_render_enter());assert(ui_render_idle());free(display_mutex);return 0;
}
