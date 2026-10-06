#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_console/app_console_uart.c"
static bool endpoint,fail_subscribe,fail_start,stop_ready=true,frozen;
static unsigned registrations,subscriptions,stops,poll_calls,help_fault;
esp_err_t app_console_endpoint_register(app_endpoint_t id,const app_endpoint_config_t *cfg)
{ assert(id==APP_ENDPOINT_UART && cfg->control_depth==8 && cfg->bulk_depth==1);++registrations;if(endpoint)return ESP_ERR_INVALID_STATE;endpoint=true;return ESP_OK; }
void app_console_get_status(app_console_status_t *status)
{ *status=(app_console_status_t){.running=true,.accepting=true,.subscriptions_frozen=frozen}; }
void app_console_endpoint_stop(app_endpoint_t id) { assert(id==APP_ENDPOINT_UART);endpoint=false;++stops; }
esp_err_t app_console_subscribe(app_message_type_t type,app_endpoint_t id)
{ assert(endpoint && id==APP_ENDPOINT_UART);assert(type==APP_MESSAGE_UI_PREFERENCES || type==APP_MESSAGE_DISPLAY_BENCH);++subscriptions;return fail_subscribe?ESP_FAIL:ESP_OK; }
esp_err_t debug_console_start_owner(debug_command_t fn,void (*tick)(void),void (*retire)(void))
{ assert(fn==command && tick==poll && retire==retire_uart);return fail_start?ESP_ERR_NO_MEM:ESP_OK; }
bool debug_console_stop(uint32_t timeout) { (void)timeout;return stop_ready; }
int debug_printf(const char *fmt,...)
{ if(strstr(fmt,"display fault"))++help_fault;return 0; }
void fake_log(const char *tag,const char *fmt,...) { (void)tag;(void)fmt; }
esp_err_t app_console_receive(app_endpoint_t id,app_message_t *event,uint32_t wait)
{ assert(id==APP_ENDPOINT_UART && !wait);(void)event;return ESP_ERR_TIMEOUT; }
void app_message_release(app_message_t *event) { (void)event; }
#define CMD(name) bool name(int argc,char **argv) { (void)argc;(void)argv;return false; }
CMD(i2c_console_command) CMD(ui_preferences_command) CMD(wifi_console_command) CMD(uart_status_command)
bool camera_commands_command(int argc,char **argv) { return argc==1 && !strcmp(argv[0],"j"); }
void wifi_console_poll(void) { assert(false); } /* Retired editor never polled. */
void i2c_console_poll(void) { ++poll_calls; }
void ui_preferences_event(const app_message_t *event) { (void)event; }
#if CONFIG_REMOTE_DBG_SIM
CMD(display_bench_command) CMD(lcd_sim_command)
void display_bench_poll(void) { ++poll_calls; }
void display_bench_event(const app_message_t *event) { (void)event; }
void lcd_sim_event(const app_message_t *event) { (void)event; }
#endif
int main(void)
{
 fail_subscribe=true;assert(app_console_uart_start()==ESP_FAIL && !endpoint && stops==1);
 fail_subscribe=false;fail_start=true;assert(app_console_uart_start()==ESP_ERR_NO_MEM && !endpoint);
 fail_start=false;assert(app_console_uart_start()==ESP_OK && endpoint);
 frozen=true;unsigned regs=registrations,subs=subscriptions;
 assert(app_console_uart_start()==ESP_ERR_INVALID_STATE && registrations==regs && endpoint);
 char *maint[]={"maint"},*probe[]={"probe"},*help[]={"help"},*j[]={"j"};
 assert(!command(1,maint) && !command(1,probe));assert(command(1,j));
 poll();assert(!help_fault && poll_calls>=1);assert(command(1,help));
#if CONFIG_REMOTE_DBG_SIM
 assert(help_fault==1);
#else
 assert(!help_fault);
#endif
 stop_ready=false;assert(!app_console_uart_quiesce(0) && !endpoint && uart_started);
 assert(app_console_uart_start()==ESP_ERR_INVALID_STATE);
 stop_ready=true;assert(app_console_uart_quiesce(100) && !uart_started);
 assert(app_console_uart_start()==ESP_OK && endpoint && subscriptions==subs);
 assert(app_console_uart_quiesce(100));
 frozen=false;assert(app_console_uart_start()==ESP_OK && subscriptions>subs);
 assert(app_console_uart_quiesce(100));return 0;
}
