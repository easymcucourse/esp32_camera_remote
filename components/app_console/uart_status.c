#include "uart_status.h"
#include "app_console.h"
#include "debug_console.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_ota_ops.h"
#include <string.h>

/* Sole UART task owns these copied envelopes; avoid ten request/reply pairs
 * on its internal 4096-byte stack. Every reply lease is released per command. */
static app_message_t requests[10],replies[10];
static esp_err_t results[10];
static const app_message_type_t types[6]={APP_MESSAGE_CAMERA_STATUS,APP_MESSAGE_UI_STATUS,
    APP_MESSAGE_INPUT_STATUS,APP_MESSAGE_CAMERA_CAPABILITIES,APP_MESSAGE_UI_PREFERENCES,APP_MESSAGE_SYSTEM_STATUS};
static const app_endpoint_t targets[6]={APP_ENDPOINT_CAMERA,APP_ENDPOINT_UI,APP_ENDPOINT_INPUT,
    APP_ENDPOINT_CAMERA,APP_ENDPOINT_UI,APP_ENDPOINT_SYSTEM};
static bool extra_status(void)
{
    int64_t deadline=esp_timer_get_time()+500000;
    uint32_t generation=app_console_endpoint_generation(APP_ENDPOINT_UART);
    for (unsigned i=0;i<10;++i) {
        requests[i]=(app_message_t){.type=i ? APP_MESSAGE_UI_PROPERTY_STATUS : APP_MESSAGE_UI_STATUS,
            .source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,.flags=APP_MESSAGE_REQUEST,
            .generation=generation,.deadline_us=deadline};
        if (i) requests[i].payload.command.index=APP_CAMERA_PROPERTY_ASPECT+i-1;
        replies[i]=(app_message_t){0};
    }
    esp_err_t err=app_console_request_many(requests,replies,results,10);
    if (err==ESP_OK) for (unsigned i=0;i<10;++i) {
        err=results[i]!=ESP_OK ? results[i] : replies[i].result;
        if (err==ESP_OK && i && replies[i].payload.property.property!=APP_CAMERA_PROPERTY_ASPECT+i-1)
            err=ESP_ERR_INVALID_RESPONSE;
        if (err!=ESP_OK) break;
    }
    if (err==ESP_OK) {
        static const char *names[]={"aspect","drive","effect","dro","af_area","wl_flash","wb_temp","wb_ab","wb_gm"};
        debug_printf("[dbg] OK extra active=%d selected=%u\n",replies[0].payload.ui.extra_menu,replies[0].payload.ui.selected);
        for (unsigned i=1;i<10;++i) {
            const app_camera_property_state_t *value=&replies[i].payload.property;
            debug_printf("[dbg] extra property=%s actual=0x%08lx writable=%d status=%u target_valid=%d target=0x%08lx\n",
                names[i-1],(unsigned long)value->actual,value->writable,value->status,value->target_valid,(unsigned long)value->target);
        }
    } else debug_printf("[dbg] ERR extra %s\n",esp_err_to_name(err));
    for (unsigned i=0;i<10;++i) app_message_release(&replies[i]);
    return true;
}
static const char *boot_status(const esp_partition_t *running)
{
    esp_ota_img_states_t state;
    if (running && esp_ota_get_state_partition(running,&state)==ESP_OK) {
        if (state==ESP_OTA_IMG_PENDING_VERIFY) return "pending_verify";
        const esp_partition_t *other=esp_ota_get_next_update_partition(NULL);esp_ota_img_states_t previous;
        if (other && esp_ota_get_state_partition(other,&previous)==ESP_OK &&
            (previous==ESP_OTA_IMG_ABORTED || previous==ESP_OTA_IMG_INVALID)) return "rolled_back";
        if (state==ESP_OTA_IMG_VALID) return "valid";
    }
    return "unknown";
}
bool uart_status_command(int argc,char **argv)
{
    if (argc==2 && !strcmp(argv[0],"extra") && !strcmp(argv[1],"status")) return extra_status();
    if (argc!=1 || strcmp(argv[0],"status")) return false;
    int64_t deadline=esp_timer_get_time()+500000;
    uint32_t generation=app_console_endpoint_generation(APP_ENDPOINT_UART);
    for (unsigned i=0;i<6;++i) {
        requests[i]=(app_message_t){.type=types[i],.source=APP_ENDPOINT_UART,.target=targets[i],
            .flags=APP_MESSAGE_REQUEST,.generation=generation,.deadline_us=deadline};
        replies[i]=(app_message_t){0};
    }
    requests[4].payload.command.index=APP_UI_PREF_GET;
    esp_err_t err=app_console_request_many(requests,replies,results,6);
    if (err!=ESP_OK) debug_printf("[dbg] ERR status %s\n",esp_err_to_name(err));
    if (err==ESP_OK) for (unsigned i=0;i<6;++i) {
        if (results[i]!=ESP_OK || replies[i].result!=ESP_OK) {
            err=results[i]!=ESP_OK ? results[i] : replies[i].result;
            debug_printf("[dbg] ERR status endpoint=%u type=%u %s\n",targets[i],types[i],esp_err_to_name(err));
            break;
        }
    }
    if (err==ESP_OK) {
        const app_camera_status_t *camera=&replies[0].payload.camera;
        const app_ui_state_t *board=&replies[1].payload.ui;
        const app_input_state_t *atom=&replies[2].payload.input;
        const gamepad_caps_t *caps=&replies[3].payload.capabilities;
        debug_printf("[dbg] menu selected=%u maint=%d\n",board->selected,replies[5].payload.system.mode==APP_SYSTEM_MODE_MAINT);
        debug_printf("[dbg] OK status camera_busy=%d session=%d stopped=%d last_io=%d phase=\"%.*s\" fps=%u.%u camera_battery=%u focus=0x%04x settings=%d display_failed=%d atom=%d protocol=2 mismatch=%d source_epoch=%lu report_id=%lu ds4=%d buttons=0x%05lx R=(%d,%d) LT=%u RT=%u free_internal=%u free_psram=%u sim=%d left_stick=not_forwarded\n",
            camera->busy,camera->session,camera->stopped,camera->last_io,(int)sizeof(camera->phase),camera->phase,
            board->fps_tenths/10,board->fps_tenths%10,board->battery,board->focus,board->settings,board->failed,
            atom->atom_online,atom->mismatch,(unsigned long)atom->source_epoch,(unsigned long)atom->report_id,
            atom->connected,(unsigned long)atom->buttons,atom->rx,atom->ry,atom->lt,atom->rt,
            (unsigned)replies[5].payload.system.free_internal,(unsigned)replies[5].payload.system.free_psram,atom->sim);
        debug_printf("[dbg] wifi default_password=%d\n",board->default_password);
        const esp_partition_t *running=esp_ota_get_running_partition();
        debug_printf("[dbg] ota running=%s state=%s\n",running ? running->label : "unknown",boot_status(running));
        unsigned level=replies[4].payload.command.value;
        debug_printf("[dbg] ui info=%s\n",level==0 ? "full" : level==1 ? "compact" : level==2 ? "hidden" : "unknown");
        debug_printf("[dbg] record known=%d recording=%d pending=%d\n",caps->recording_known,caps->recording,caps->record_pending);
        debug_printf("[dbg] ev actual=%ld (milli-EV; unknown=-2147483648)\n",(long)board->ev);
        debug_printf("[dbg] controls pad_type=%s lens=%u zoom_known=%d zoom_enabled=%d zoom_available=%d\n",
            replies[4].payload.command.direction ? "xbox" : "ds",caps->lens,caps->zoom_known,caps->zoom_enabled,gamepad_zoom_available(caps));
        debug_printf("[dbg] heap min_internal=%u min_psram=%u largest_internal=%u largest_psram=%u\n",
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
    }
    for (unsigned i=0;i<6;++i) app_message_release(&replies[i]);
    return true;
}
