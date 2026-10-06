#include "ui_wifi_menu.h"
#include "ui_wifi_menu_kernel_noreset.h"
#include "app_ui.h"
#include "app_console.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>

typedef struct {
    pad_action_t action;
    app_endpoint_t source;
    uint32_t generation, endpoint_epoch;
    int64_t deadline_us;
} menu_job_t;
_Static_assert(sizeof(menu_job_t)<=48,"UI menu admission metadata must stay bounded");
static QueueHandle_t inputs;
static atomic_bool owns_page,open_pending,cancel_page;
static atomic_bool running,closing,accepting;
static atomic_uint admissions;
static esp_err_t job_error(const menu_job_t *job)
{
    if (atomic_load(&closing)) return ESP_ERR_INVALID_STATE;
    if (job->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    return job->endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI) &&
        job->generation==app_console_endpoint_generation(job->source) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
bool ui_wifi_menu_active(void) { return atomic_load(&owns_page); }
static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time()/1000); }
static esp_err_t action_open(const app_message_t *message)
{
    if (!inputs) return ESP_ERR_INVALID_STATE;
    pad_action_t action=message->payload.action;
    if (action.type==PAD_ACTION_RELEASE_ALL) { atomic_store(&cancel_page,true);return ESP_OK; }
    if (atomic_load(&cancel_page)) return ESP_ERR_INVALID_STATE;
    bool opening=!ui_wifi_menu_active() && app_ui_menu_selected()==7 &&
        (action.type==PAD_ACTION_MENU_CONFIRM || (action.type==PAD_ACTION_MENU_STEP && action.value>0));
    if (!opening && !ui_wifi_menu_active()) return ESP_OK;
    if (opening) { atomic_store(&open_pending,true);atomic_store(&owns_page,true); }
    menu_job_t job={action,message->source,message->generation,message->endpoint_epoch,message->deadline_us};
    if (xQueueSend(inputs,&job,0)==pdTRUE) return ESP_OK;
    if (opening) { atomic_store(&open_pending,false);atomic_store(&owns_page,false); }
    return ESP_ERR_NO_MEM;
}
esp_err_t ui_wifi_menu_action(const app_message_t *message)
{
    if (!message) return ESP_ERR_INVALID_ARG;
    atomic_fetch_add(&admissions,1);
    esp_err_t result=atomic_load(&closing) || !atomic_load(&accepting) ? ESP_ERR_INVALID_STATE : action_open(message);
    atomic_fetch_sub(&admissions,1);return result;
}
static bool cancelled(void *context) { (void)context;return atomic_load(&closing); }
static esp_err_t rpc(app_endpoint_t target,app_message_type_t type,
    const app_message_payload_t *payload,app_message_t *reply,int64_t deadline)
{
    if (atomic_load(&closing)) return ESP_ERR_INVALID_STATE;
    app_message_t request={.type=type,.source=APP_ENDPOINT_UI,.target=target,
        .flags=APP_MESSAGE_REQUEST,.generation=app_console_endpoint_generation(APP_ENDPOINT_UI),
        .deadline_us=deadline};
    if (payload) request.payload=*payload;
    return app_console_request_cancelable(&request,reply,cancelled,NULL);
}
static void copy_value(network_config_t *out,const app_network_config_t *in)
{
    *out=(network_config_t){.channel=in->channel,.show_password=in->show_password};
    memcpy(out->ssid,in->ssid,sizeof(out->ssid));
    memcpy(out->password,in->password,sizeof(out->password));
}
static esp_err_t snapshot(network_config_t *config,unsigned *max_channel,int64_t deadline)
{
    app_message_t reply={0};
    esp_err_t err=rpc(APP_ENDPOINT_WIFI,APP_MESSAGE_WIFI_STATUS,NULL,&reply,deadline);
    if (err==ESP_OK) {
        err=reply.result;
        if (err==ESP_OK) {
            copy_value(config,&reply.payload.network.config);
            unsigned limit=reply.payload.network.max_channel;
            if (limit<1 || limit>13 || network_config_check(config,limit)!=NETWORK_CFG_OK)
                err=ESP_ERR_INVALID_RESPONSE;
            else if (max_channel) *max_channel=limit;
        }
    }
    app_message_release(&reply);return err;
}
static esp_err_t submit(const network_config_t *config,uint32_t *token,int64_t deadline)
{
    uint32_t source_epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);
    uint32_t target_epoch=app_console_endpoint_generation(APP_ENDPOINT_WIFI);
    app_message_payload_t payload={.config={.channel=0}};
    {
        memcpy(payload.config.ssid,config->ssid,sizeof(config->ssid));
        memcpy(payload.config.password,config->password,sizeof(config->password));
        payload.config.channel=config->channel;payload.config.show_password=config->show_password;
    }
    app_message_t reply={0};
    esp_err_t err=rpc(APP_ENDPOINT_WIFI,
        APP_MESSAGE_WIFI_CONFIG_PREPARE,&payload,&reply,deadline);
    if (err==ESP_OK) {
        err=reply.result;
        if (err==ESP_OK) { *token=reply.payload.command.token;if (!*token) err=ESP_ERR_INVALID_RESPONSE; }
    }
    app_message_release(&reply);
    if (err!=ESP_OK) return err;
    payload=(app_message_payload_t){.command={.token=*token}};
    err=rpc(APP_ENDPOINT_WIFI,APP_MESSAGE_WIFI_CONFIG_COMMIT,&payload,&reply,deadline);
    if (err==ESP_OK) err=reply.result;
    app_message_release(&reply);
    if (err!=ESP_OK && source_epoch==app_console_endpoint_generation(APP_ENDPOINT_UI) &&
        target_epoch==app_console_endpoint_generation(APP_ENDPOINT_WIFI)) {
        /* A timed-out COMMIT may already be running. Keep its token and poll
         * the definitive result; cancellation only affects a staged job. */
        app_message_t cancel={.type=APP_MESSAGE_WIFI_CONFIG_CANCEL,.source=APP_ENDPOINT_UI,
            .target=APP_ENDPOINT_WIFI,.generation=source_epoch,.endpoint_epoch=target_epoch,
            .deadline_us=esp_timer_get_time()+500000};
        cancel.payload.command.token=*token;app_console_send(&cancel);
    }
    return err;
}
static void publish(const wifi_menu_t *m, const char *status)
{
    app_ui_wifi_menu_view_t v = {.active = m->active, .selected = m->row};
    snprintf(v.lines[0], sizeof(v.lines[0]), "SSID %s%s", m->editing ? m->edit : m->draft.ssid,
        strcmp(m->draft.ssid, m->actual.ssid) ? " *" : "");
    snprintf(v.lines[1], sizeof(v.lines[1]), "PASS %s%s%s", m->draft.show_password ? m->draft.password : "********",
        strcmp(m->draft.password, m->actual.password) ? " *" : "",network_config_uses_default_password(&m->draft)?" DEFAULT":"");
    snprintf(v.lines[2], sizeof(v.lines[2]), "NEW PASSWORD");
    snprintf(v.lines[3], sizeof(v.lines[3]), "CHANNEL < %u >%s", m->draft.channel,
        m->draft.channel != m->actual.channel ? " *" : "");
    snprintf(v.lines[4], sizeof(v.lines[4]), "SHOW PASS %s", m->draft.show_password ? "ON" : "OFF");
    snprintf(v.lines[5], sizeof(v.lines[5]), m->pending ? "APPLYING..." : network_config_equal(&m->draft, &m->actual) ? "APPLY (no changes)" : "APPLY *");
    snprintf(v.lines[6], sizeof(v.lines[6]), "BACK");
    if (m->editing) snprintf(v.footer, sizeof(v.footer), "%sChar %u [%c] U/D change L/R move A done B cancel",
        !*m->edit ? "SSID empty! " : "", m->cursor + 1, m->edit[m->cursor] ? m->edit[m->cursor] : '~');
    else snprintf(v.footer, sizeof(v.footer), "%s", status);
    app_ui_set_wifi_menu(&v);
    if (m->active) atomic_store(&owns_page, true);
    else if (!atomic_load(&open_pending)) atomic_store(&owns_page, false);
}
static void task(void *unused)
{
    (void)unused;
    wifi_menu_t menu={0};
    uint32_t token=0,token_epoch=0,token_target_epoch=0,connection_generation=app_ui_connection_generation();
    bool display_only=false;
    char status[96]="A confirm / B back";
    while (!atomic_load(&closing)) {
        bool changed=false;
        if (atomic_exchange(&cancel_page,false)) {
            xQueueReset(inputs);atomic_store(&open_pending,false);
            menu.active=menu.editing=false;changed=true;
        }
        menu_job_t job;
        if (xQueueReceive(inputs,&job,pdMS_TO_TICKS(50))==pdTRUE) {
            bool valid=job_error(&job)==ESP_OK;
            pad_action_t action=job.action;
            if (action.type==PAD_ACTION_MENU_CONFIRM || action.type==PAD_ACTION_MENU_STEP)
                atomic_store(&open_pending,false);
            if (!valid || !app_ui_settings_mode()) menu.active=false;
            else if (!menu.active) {
                if (app_ui_menu_selected()==7 && (action.type==PAD_ACTION_MENU_CONFIRM ||
                    (action.type==PAD_ACTION_MENU_STEP && action.value>0))) {
                    network_config_t config;unsigned max_channel;
                    esp_err_t err=snapshot(&config,&max_channel,job.deadline_us);
                    if (err==ESP_OK) err=job_error(&job);
                    if (err==ESP_OK) { wifi_menu_open(&menu,&config,max_channel);menu.pending=token!=0; }
                    snprintf(status,sizeof(status),"%s",err==ESP_OK ? "A confirm / B back" : esp_err_to_name(err));
                }
            } else {
                wifi_menu_input_t input=action.type==PAD_ACTION_MENU_MOVE ? WIFI_MENU_MOVE :
                    action.type==PAD_ACTION_MENU_STEP ? WIFI_MENU_STEP :
                    action.type==PAD_ACTION_MENU_CONFIRM ? WIFI_MENU_CONFIRM : WIFI_MENU_BACK;
                wifi_menu_effect_t effect=wifi_menu_input(&menu,input,action.value,now_ms());
                if (effect==WIFI_MENU_RANDOM) network_config_make_password(menu.draft.password,esp_fill_random);
                else if (effect==WIFI_MENU_APPLY || effect==WIFI_MENU_DISPLAY) {
                    network_config_t current;
                    esp_err_t err=snapshot(&current,NULL,job.deadline_us);
                    if (err==ESP_OK) err=job_error(&job);
                    if (err!=ESP_OK) snprintf(status,sizeof(status),"%s",esp_err_to_name(err));
                    else if (!network_config_equal(&current,&menu.actual)) {
                        wifi_menu_complete(&menu,&current,false,false);
                        snprintf(status,sizeof(status),"Config changed; review draft");
                    } else {
                        display_only=effect==WIFI_MENU_DISPLAY;
                        network_config_t next=display_only ? current : menu.draft;
                        if (display_only) next.show_password=menu.draft.show_password;
                        token=0;token_epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);token_target_epoch=app_console_endpoint_generation(APP_ENDPOINT_WIFI);err=submit(&next,&token,job.deadline_us);menu.pending=token!=0;
                        snprintf(status,sizeof(status),"%s",err==ESP_OK ? "Saving..." : esp_err_to_name(err));
                        if (!token) menu.draft.show_password=menu.actual.show_password;
                    }
                }
            }
            changed=true;
        }
        if (atomic_load(&closing)) break;
        if (token && (token_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI) ||
            token_target_epoch!=app_console_endpoint_generation(APP_ENDPOINT_WIFI))) {
            token=0;menu.pending=false;changed=true;
            snprintf(status,sizeof(status),"Config endpoint restarted; refresh required");
        }
        if (token) {
            app_message_payload_t payload={.command={.token=token}};
            app_message_t reply={0};
            esp_err_t transport=rpc(APP_ENDPOINT_WIFI,
                APP_MESSAGE_WIFI_CONFIG_RESULT,&payload,&reply,esp_timer_get_time()+500000);
            if (transport==ESP_OK && reply.result!=ESP_ERR_NOT_FINISHED) {
                esp_err_t result=reply.result==ESP_OK ? (esp_err_t)reply.payload.command.value : reply.result;
                network_config_t current;
                esp_err_t fetched=snapshot(&current,NULL,esp_timer_get_time()+500000);
                if (fetched==ESP_OK) {
                    wifi_menu_complete(&menu,&current,result==ESP_OK,display_only);
                    snprintf(status,sizeof(status),"%s",result==ESP_OK ? (display_only ? "Password display saved" : "Reconnect camera to Wi-Fi") : esp_err_to_name(result));
                    token=0;changed=true;
                }
            }
            app_message_release(&reply);
        }
        uint32_t connection=app_ui_connection_generation();
        if (connection!=connection_generation || !app_ui_settings_mode()) {
            connection_generation=connection;
            if (menu.active) { menu.active=menu.editing=false;changed=true; }
        }
        if (atomic_load(&closing)) break;
        if (changed) publish(&menu,status);
        app_ui_refresh_wifi_info();
    }
    /* A committed config/reset belongs to its domain, not to this menu task.
     * Cancel any known staged Wi-Fi token; Core must separately drain domain
     * workers before exclusive maintenance or router shutdown. */
    while (token) {
        uint32_t epoch=app_console_endpoint_generation(APP_ENDPOINT_UI);
        if (!epoch || epoch!=token_epoch || token_target_epoch!=app_console_endpoint_generation(APP_ENDPOINT_WIFI)) break;
        app_message_t cancel={.type=APP_MESSAGE_WIFI_CONFIG_CANCEL,.source=APP_ENDPOINT_UI,
            .target=APP_ENDPOINT_WIFI,.generation=token_epoch,.endpoint_epoch=token_target_epoch,.deadline_us=esp_timer_get_time()+500000};
        cancel.payload.command.token=token;
        if (app_console_send(&cancel)==ESP_OK) break;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    while (atomic_load(&admissions)) vTaskDelay(pdMS_TO_TICKS(10));
    atomic_store(&open_pending,false);atomic_store(&owns_page,false);atomic_store(&cancel_page,false);
    app_ui_wifi_menu_view_t cleared={0};app_ui_set_wifi_menu(&cleared);
    app_ui_refresh_wifi_info();
    atomic_store(&running,false);vTaskDelete(NULL);
}
bool ui_wifi_menu_quiesce(uint32_t timeout_ms)
{
    atomic_store(&accepting,false);atomic_store(&closing,true);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&running) || atomic_load(&admissions)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    if (inputs) vQueueDelete(inputs);
    inputs=NULL;return true;
}
esp_err_t ui_wifi_menu_start(void)
{
    if (atomic_load(&running)) return atomic_load(&closing) ? ESP_ERR_INVALID_STATE : ESP_OK;
    if (inputs) return ESP_ERR_INVALID_STATE;
    atomic_store(&closing,false);atomic_store(&accepting,false);
    inputs=xQueueCreate(16,sizeof(menu_job_t));
    if (!inputs) return ESP_ERR_NO_MEM;
    atomic_store(&running,true);
    if (xTaskCreate(task,"wifi_menu",4096,NULL,2,NULL)!=pdPASS) {
        vQueueDelete(inputs);inputs=NULL;atomic_store(&running,false);return ESP_ERR_NO_MEM;
    }
    atomic_store(&accepting,true);return ESP_OK;
}
