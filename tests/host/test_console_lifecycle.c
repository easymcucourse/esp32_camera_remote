#include <assert.h>
#include <string.h>
#ifdef _WIN32
#define flockfile _lock_file
#define funlockfile _unlock_file
#endif
#include "../../common/debug_console.c"
static TaskFunction_t task;
static unsigned installed,deleted,commands,polls,delete_failures,retired;
static bool install_fail,create_fail,run_on_delay;
static TickType_t ticks;
static const char *input;
static bool dispatch(int argc,char **argv) { assert(argc==1);assert(!strcmp(argv[0],"unknown"));++commands;return false; }
static void poll(void) { ++polls; }
static void retire(void) { ++retired; }
esp_err_t uart_driver_install(int id,unsigned rx,unsigned tx,unsigned depth,void *queue,unsigned flags)
{ assert(id==0 && rx==512 && !tx && !depth && !queue && !flags);++installed;return install_fail?ESP_FAIL:ESP_OK; }
esp_err_t uart_driver_delete(int id)
{ assert(id==0);++deleted;if(delete_failures){--delete_failures;return ESP_FAIL;}return ESP_OK; }
int uart_read_bytes(int id,void *byte,unsigned length,TickType_t timeout)
{ assert(id==0 && length==1 && timeout==20);if(input && *input){*(char*)byte=*input++;return 1;}return -1; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,unsigned stack,void *ctx,unsigned priority,void *handle)
{ assert(!strcmp(name,"debug_console") && stack==4096 && priority==2 && !ctx && !handle);if(create_fail)return 0;task=fn;return pdPASS; }
void vTaskDelay(TickType_t delay) { ticks+=delay;if(run_on_delay && task){TaskFunction_t fn=task;task=NULL;fn(NULL);} }
TickType_t xTaskGetTickCount(void) { return ticks; }
void vTaskDelete(void *t) { assert(!t); }
void esp_log_level_set(const char *tag,esp_log_level_t level) { (void)tag;(void)level; }
const esp_app_desc_t *esp_app_get_description(void) { static const esp_app_desc_t desc={"test","v","d","t","idf"};return &desc; }
int main(void)
{
 assert(debug_console_stop(0));
 install_fail=true;assert(debug_console_start(dispatch,poll)==ESP_FAIL);assert(!atomic_load(&started));
 install_fail=false;create_fail=true;delete_failures=2;
 assert(debug_console_start(dispatch,poll)==ESP_ERR_NO_MEM && atomic_load(&started) && !atomic_load(&worker_exists));
 assert(!debug_console_stop(0));assert(debug_console_stop(0));
 create_fail=false;assert(debug_console_start(dispatch,poll)==ESP_OK);
 unsigned count=installed;assert(debug_console_start(dispatch,poll)==ESP_ERR_INVALID_STATE && installed==count);
 assert(!debug_console_stop(0) && atomic_load(&started) && task);
 assert(!debug_console_stop(10) && atomic_load(&started) && task);
 run_on_delay=true;assert(debug_console_stop(100) && !atomic_load(&started) && !task);
 assert(!commands);run_on_delay=false;
 assert(debug_console_start_owner(dispatch,poll,retire)==ESP_OK);input="unknown\n";
 TaskFunction_t fn=task;task=NULL;fn(NULL);
 assert(retired==1);
 assert(commands==1 && polls>=2 && !atomic_load(&started) && !application_command);
 assert(debug_console_start(dispatch,poll)==ESP_OK);run_on_delay=true;delete_failures=2;
 assert(debug_console_stop(100) && !atomic_load(&started));
 assert(deleted>=6);return 0;
}
