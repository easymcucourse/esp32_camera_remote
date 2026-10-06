#include "app_input.h"
#include "app_console.h"
#include "input_owner.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>

/* Only this task accesses the owner/capabilities. Lifecycle flags are shared
 * with Core; no provider receives a callback into Camera or UI. */
static input_owner_t owner;
static gamepad_caps_t caps;
static app_ui_state_t ui;
static atomic_bool running, stopping;
static unsigned pad_kind;
static bool pad_dirty[2], preferences_known, cancel_pending;
static uint32_t provider_epochs[2];
static uint32_t camera_epoch, last_report_ms;

static uint32_t now_ms(void) { return (uint32_t)((uint64_t)esp_timer_get_time()/1000); }
static bool cancelled(void *unused) { (void)unused;return atomic_load(&stopping); }
static app_message_t message(app_message_type_t type, app_endpoint_t target)
{
    return (app_message_t){.type=type,.source=APP_ENDPOINT_INPUT,.target=target,
        .generation=app_console_endpoint_generation(APP_ENDPOINT_INPUT),
        .deadline_us=esp_timer_get_time()+100000};
}
static esp_err_t request(app_message_t *m, app_message_t *reply, bool safety)
{
    uint32_t epoch=app_console_endpoint_generation(m->target);
    m->flags=APP_MESSAGE_REQUEST;
    esp_err_t err=safety ? app_console_request(m,reply) :
        app_console_request_cancelable(m,reply,cancelled,NULL);
    if (err==ESP_OK && epoch!=app_console_endpoint_generation(m->target)) {
        app_message_release(reply);return ESP_ERR_INVALID_STATE;
    }
    return err;
}
static void flush_cancel(void)
{
    if (!cancel_pending) return;
    app_message_t m=message(APP_MESSAGE_UI_MENU_ACTION,APP_ENDPOINT_UI);
    m.deadline_us=0;m.payload.action=(pad_action_t){.type=PAD_ACTION_RELEASE_ALL};
    if (app_console_send(&m)==ESP_OK) cancel_pending=false;
}
static bool action(void *unused, pad_action_t a)
{
    (void)unused;
    bool safety=a.type==PAD_ACTION_RELEASE_ALL || a.type==PAD_ACTION_MF_CANCEL;
    if (!safety && atomic_load(&stopping)) return false;
    if (a.type==PAD_ACTION_MAINT_TOGGLE) return true;
    if (a.type==PAD_ACTION_UI_INFO_NEXT) return true; /* Persisted settings are Web-only. */
    if (a.type==PAD_ACTION_UI_TOGGLE || a.type==PAD_ACTION_MENU_MOVE ||
        a.type==PAD_ACTION_MENU_STEP || a.type==PAD_ACTION_MENU_CONFIRM || a.type==PAD_ACTION_MENU_BACK) {
        app_message_t m=message(APP_MESSAGE_UI_MENU_ACTION,APP_ENDPOINT_UI),reply={0};
        /* UI's sole consumer finishes the current JPEG before menu dispatch.
         * Real decode/publication takes 200-300ms; the generic 100ms query
         * budget would discard a valid button before it reaches that owner.
         * Bound just menu work to one frame plus dispatch, without extending
         * Camera actuator/release or periodic snapshot deadlines. */
        m.deadline_us=esp_timer_get_time()+500000;
        m.payload.action=a;
        if (request(&m,&reply,false)!=ESP_OK) return false;
        esp_err_t err=reply.result;app_ui_menu_result_t result=reply.payload.menu;
        app_message_release(&reply);
        if (err!=ESP_OK) return false;
        ui=result.state;caps.settings=ui.settings;
        if (result.route!=APP_UI_MENU_CAMERA || !caps.session) return true;
        m=message(APP_MESSAGE_CAMERA_MENU_ACTION,APP_ENDPOINT_CAMERA);
        m.payload.command.index=result.property;m.payload.command.direction=a.value;
        m.payload.command.token=a.generation;
        if (request(&m,&reply,false)!=ESP_OK) return false;
        err=reply.result;app_message_release(&reply);return err==ESP_OK;
    }
    if (a.type==PAD_ACTION_RELEASE_ALL) { cancel_pending=true;flush_cancel(); }
    app_message_t m=message(APP_MESSAGE_CAMERA_ACTION,APP_ENDPOINT_CAMERA),reply={0};
    if (safety) a.generation=caps.generation;
    m.payload.action=a;
    if (request(&m,&reply,safety)!=ESP_OK) { caps.session=false;return false; }
    esp_err_t err=reply.result;
    /* A handler may reject an expired envelope before producing capabilities.
     * An empty error payload is never proof that Camera is safely offline. */
    if (!reply.payload.capabilities.generation) {
        app_message_release(&reply);caps.session=false;return false;
    }
    caps=reply.payload.capabilities;caps.settings=ui.settings;
    app_message_release(&reply);
    /* An offline owner has no actuator holds. When live, admission failure
     * retains the kernel's reserved safety retry instead of rearming. */
    return err==ESP_OK || (safety && !caps.session);
}
static void refresh_camera(void)
{
    uint32_t epoch=app_console_endpoint_generation(APP_ENDPOINT_CAMERA);
    if (epoch!=camera_epoch) {
        caps=(gamepad_caps_t){.settings=ui.settings};camera_epoch=epoch;
        input_reports_disconnect(&owner.reports,owner.reports.selected);
    }
    app_message_t m=message(APP_MESSAGE_CAMERA_CAPABILITIES,APP_ENDPOINT_CAMERA),reply={0};
    if (request(&m,&reply,false)==ESP_OK) {
        if (reply.result==ESP_OK && reply.payload.capabilities.generation) { caps=reply.payload.capabilities;caps.settings=ui.settings; }
        else caps.session=false;
        app_message_release(&reply);
    } else caps.session=false;
}
static void refresh_ui(void)
{
    const app_endpoint_t targets[2]={APP_ENDPOINT_INPUT_ATOM,APP_ENDPOINT_INPUT_SIM};
#if CONFIG_REMOTE_DBG_SIM
    const unsigned provider_count=2;
#else
    const unsigned provider_count=1;
#endif
    for (unsigned i=0;i<provider_count;++i) {
        uint32_t epoch=app_console_endpoint_generation(targets[i]);
        if (epoch!=provider_epochs[i]) { provider_epochs[i]=epoch;pad_dirty[i]=true; }
    }
    app_message_t m=message(APP_MESSAGE_UI_STATUS,APP_ENDPOINT_UI),reply={0};
    if (request(&m,&reply,false)==ESP_OK) {
        if (reply.result==ESP_OK) { ui=reply.payload.ui;caps.settings=ui.settings; }
        app_message_release(&reply);
    }
    m=message(APP_MESSAGE_UI_PREFERENCES,APP_ENDPOINT_UI);
    m.payload.command.index=APP_UI_PREF_GET;
    if (request(&m,&reply,false)==ESP_OK) {
        if (reply.result==ESP_OK && reply.payload.command.direction>=0 && reply.payload.command.direction<=1) {
            preferences_known=true;
            unsigned kind=(unsigned)reply.payload.command.direction;
            if (kind!=pad_kind) { pad_kind=kind;pad_dirty[0]=pad_dirty[1]=true; }
        }
        app_message_release(&reply);
    }
    for (unsigned i=0;i<provider_count;++i) if (pad_dirty[i] && preferences_known && provider_epochs[i]) {
        m=message(i?APP_MESSAGE_INPUT_SIM_COMMAND:APP_MESSAGE_INPUT_ATOM_COMMAND,targets[i]);
        m.payload.command.index=APP_INPUT_PROVIDER_PAD_KIND;m.payload.command.value=pad_kind;
        if (app_console_send(&m)==ESP_OK) pad_dirty[i]=false;
    }
}
static app_input_state_t snapshot(void)
{
    const input_report_t *r=&owner.latest;
    return (app_input_state_t){.connected=r->connected && owner.reports.gamepad.connected,
        .atom_online=r->atom_online,.sim=r->sim,.mismatch=r->mismatch,.buttons=r->buttons,
        .source_epoch=r->source_epoch,.report_id=r->report_id,.rx=r->rx,.ry=r->ry,
        .lt=r->lt,.rt=r->rt,.battery=r->battery,.kind=r->source_epoch?r->kind:pad_kind,.gimbal=r->gimbal};
}
static void publish(void)
{
    app_message_t m=message(APP_MESSAGE_INPUT_STATE,APP_ENDPOINT_NONE);
    m.flags=APP_MESSAGE_EVENT;m.payload.input=snapshot();app_console_send(&m);
}
static void receive(void)
{
    app_message_t m;
    for (unsigned i=0;i<8 && app_console_receive(APP_ENDPOINT_INPUT,&m,0)==ESP_OK;++i) {
        app_message_t reply={0};
        esp_err_t err=ESP_ERR_INVALID_ARG;
        bool valid=!m.lease && m.flags==APP_MESSAGE_REQUEST && m.generation &&
            m.generation==app_console_endpoint_generation(m.source) &&
            m.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_INPUT) &&
            m.deadline_us>esp_timer_get_time();
        if (valid && m.type==APP_MESSAGE_INPUT_STATUS) { reply.payload.input=snapshot();err=ESP_OK; }
        else if (valid && m.type==APP_MESSAGE_INPUT_SELECT &&
            (m.source==APP_ENDPOINT_UART || m.source==APP_ENDPOINT_SYSTEM) &&
            m.payload.command.index<INPUT_SOURCE_COUNT && !atomic_load(&stopping)) {
            /* Selection persists even when a safety retry blocks rearming. */
#if !CONFIG_REMOTE_DBG_SIM
            if (m.payload.command.index==INPUT_SOURCE_UART_SIM) err=ESP_ERR_NOT_SUPPORTED;
            else
#endif
            {
            input_owner_select(&owner,(input_source_kind_t)m.payload.command.index);err=ESP_OK;
            }
        }
        reply.result=err;
        if (m.flags==APP_MESSAGE_REQUEST) app_console_reply(&m,&reply);
        app_message_release(&m);
    }
}
static void task(void *unused)
{
    (void)unused;uint32_t last_ui=now_ms()-250;
    input_owner_init(&owner,action,NULL);
    for (;;) {
        TickType_t cycle=xTaskGetTickCount();uint32_t now=now_ms();
        if (atomic_load(&stopping)) {
            input_owner_quiesce(&owner);input_owner_tick(&owner,&caps,now);flush_cancel();publish();
            if (!owner.reports.release_pending && !owner.reports.mf_pending &&
                !cancel_pending && input_provider_registry_idle()) break;
        } else {
            receive();refresh_camera();
            if ((uint32_t)(now-last_ui)>=250) { refresh_ui();last_ui=now; }
            uint32_t old_id=owner.latest.report_id,old_epoch=owner.latest.source_epoch;
            input_owner_tick(&owner,&caps,now_ms());
            if (old_id!=owner.latest.report_id || old_epoch!=owner.latest.source_epoch) last_report_ms=now_ms();
            if (owner.latest.connected && (uint32_t)(now_ms()-last_report_ms)>1000) {
                input_reports_disconnect(&owner.reports,owner.reports.selected);
                owner.latest=(input_report_t){.battery=255};
            }
            flush_cancel();publish();
        }
        vTaskDelayUntil(&cycle,pdMS_TO_TICKS(50));
    }
    app_console_endpoint_stop(APP_ENDPOINT_INPUT);
    atomic_store(&running,false);vTaskDelete(NULL);
}
esp_err_t app_input_start(void)
{
    if (atomic_load(&running)) return ESP_ERR_INVALID_STATE;
    /* A stopped registry cannot restart while a previous provider still owns
     * a registration. No old handle can publish into the new lifetime. */
    esp_err_t err=input_provider_registry_deinit();
    if (err!=ESP_OK) return err;
    err=input_provider_registry_init();if (err!=ESP_OK) return err;
    const app_endpoint_config_t config={8,1};
    err=app_console_endpoint_register(APP_ENDPOINT_INPUT,&config);
    if (err!=ESP_OK) { input_provider_registry_close();input_provider_registry_deinit();return err; }
    caps=(gamepad_caps_t){0};ui=(app_ui_state_t){0};camera_epoch=0;
    pad_kind=0;pad_dirty[0]=pad_dirty[1]=true;preferences_known=false;
    provider_epochs[0]=provider_epochs[1]=0;
    cancel_pending=false;last_report_ms=now_ms();
    atomic_store(&stopping,false);atomic_store(&running,true);
    if (xTaskCreate(task,"input_owner",4096,NULL,4,NULL)!=pdPASS) {
        atomic_store(&running,false);app_console_endpoint_stop(APP_ENDPOINT_INPUT);
        input_provider_registry_close();input_provider_registry_deinit();return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t app_input_quiesce(uint32_t timeout_ms)
{
    if (!atomic_load(&running)) return ESP_OK;
    input_provider_registry_close();atomic_store(&stopping,true);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (atomic_load(&running)) {
        if (esp_timer_get_time()>=deadline) return ESP_ERR_TIMEOUT;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}
