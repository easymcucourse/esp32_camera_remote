#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "../../components/app_console/uart_status.c"
static unsigned calls,releases;
static int failed=-1;
static bool transport_fail;
static char output[8192];
static const esp_partition_t run={"ota_0"},other={"ota_1"};
static esp_ota_img_states_t current_state=ESP_OTA_IMG_VALID,previous_state=ESP_OTA_IMG_VALID;
int64_t esp_timer_get_time(void) { return 1000; }
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint) { assert(endpoint==APP_ENDPOINT_UART);return 7; }
const char *esp_err_to_name(esp_err_t err) { return err==ESP_OK ? "ESP_OK" : "ERROR"; }
const esp_partition_t *esp_ota_get_running_partition(void) { return &run; }
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *partition) { assert(!partition);return &other; }
esp_err_t esp_ota_get_state_partition(const esp_partition_t *partition,esp_ota_img_states_t *state)
{ *state=partition==&run ? current_state : previous_state;return ESP_OK; }
size_t heap_caps_get_minimum_free_size(unsigned caps) { return caps; }
size_t heap_caps_get_largest_free_block(unsigned caps) { return caps+10; }
int debug_printf(const char *format,...)
{
    va_list args;va_start(args,format);
    int n=vsnprintf(output+strlen(output),sizeof(output)-strlen(output),format,args);
    va_end(args);return n;
}
void app_message_release(app_message_t *message) { ++releases;message->lease=NULL; }
esp_err_t app_console_request_many(app_message_t *rq,app_message_t *rp,esp_err_t *errors,size_t count)
{
    ++calls;assert(count==6 || count==10);
    if (count==10) {
        for (unsigned i=0;i<10;++i) {
            assert(rq[i].source==APP_ENDPOINT_UART && rq[i].target==APP_ENDPOINT_UI &&
                rq[i].deadline_us==501000 && rq[i].generation==7);
            errors[i]=ESP_OK;rp[i].result=ESP_OK;
            if (i) {
                assert(rq[i].type==APP_MESSAGE_UI_PROPERTY_STATUS && rq[i].payload.command.index==APP_CAMERA_PROPERTY_ASPECT+i-1);
                rp[i].payload.property=(app_camera_property_state_t){.property=APP_CAMERA_PROPERTY_ASPECT+i-1,.actual=i,.writable=true};
            } else { assert(rq[i].type==APP_MESSAGE_UI_STATUS);rp[i].payload.ui.extra_menu=true; }
        }
        return ESP_OK;
    }
    for (unsigned i=0;i<count;++i) {
        assert(rq[i].source==APP_ENDPOINT_UART && rq[i].target==targets[i] && rq[i].type==types[i] &&
            rq[i].flags==APP_MESSAGE_REQUEST && rq[i].generation==7 && rq[i].deadline_us==501000);
        errors[i]=(int)i==failed && transport_fail ? ESP_ERR_TIMEOUT : ESP_OK;
        rp[i].result=(int)i==failed && !transport_fail ? ESP_ERR_INVALID_STATE : ESP_OK;
    }
    assert(rq[4].payload.command.index==APP_UI_PREF_GET);
    rp[0].payload.camera=(app_camera_status_t){.session=true,.phase="LIVE"};
    rp[1].payload.ui=(app_ui_state_t){.fps_tenths=234,.ev=-700,.selected=4,.battery=88};
    rp[2].payload.input=(app_input_state_t){.connected=true,.sim=true,.source_epoch=8,.report_id=9};
    rp[3].payload.capabilities=(gamepad_caps_t){.lens=PAD_LENS_POWER_ZOOM,.recording_known=true,.record_pending=true};
    rp[4].payload.command.value=1;rp[4].payload.command.direction=1;
    rp[5].payload.system.free_internal=1234;rp[5].payload.system.free_psram=5678;
    return ESP_OK;
}
static void status(void)
{ output[0]=0;releases=0;char *argv[]={"status"};assert(uart_status_command(1,argv) && releases==6); }
int main(void)
{
    char *other_command[]={"help"};assert(!uart_status_command(1,other_command) && !calls);
    status();assert(strstr(output,"OK status") && strstr(output,"fps=23.4") && strstr(output,"ev actual=-700") &&
        strstr(output,"atom=0") && strstr(output,"sim=1") && strstr(output,"free_internal=1234") &&
        strstr(output,"info=compact") && strstr(output,"pad_type=xbox") && strstr(output,"running=ota_0 state=valid"));
    current_state=ESP_OTA_IMG_PENDING_VERIFY;status();assert(strstr(output,"state=pending_verify"));
    current_state=ESP_OTA_IMG_VALID;previous_state=ESP_OTA_IMG_ABORTED;status();assert(strstr(output,"state=rolled_back"));
    for (int i=0;i<6;++i) {
        failed=i;transport_fail=false;status();assert(strstr(output,"ERR status") && !strstr(output,"OK status"));
        transport_fail=true;status();assert(strstr(output,"ERR status") && !strstr(output,"OK status"));
    }
    char *extra[]={"extra","status"};output[0]=0;releases=0;
    assert(uart_status_command(2,extra) && releases==10);
    assert(strstr(output,"extra active=1") && strstr(output,"property=aspect") && strstr(output,"property=wb_gm"));
    puts("UART parallel domain snapshots and partial failure formatting passed");return 0;
}
