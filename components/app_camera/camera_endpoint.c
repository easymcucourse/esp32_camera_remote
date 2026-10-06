#include "camera_endpoint.h"
#include "camera_runtime.h"
#include "sdkconfig.h"
#include "app_console.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdatomic.h>

static QueueHandle_t results;
static app_message_t stops[APP_CONSOLE_PENDING_CAPACITY];
static uint32_t stop_lifetimes[APP_CONSOLE_PENDING_CAPACITY];
static unsigned stop_count;
static atomic_bool endpoint_done;
static bool network_subscribed;
#if CONFIG_REMOTE_DBG_SIM
static struct { app_message_t origin,release;uint32_t token;bool ready,resume,releasing; } display_session;
static void finish_display(void)
{
    if (!display_session.token) return;
    bool current=display_session.origin.generation==app_console_endpoint_generation(APP_ENDPOINT_UI) &&
        display_session.origin.endpoint_epoch==app_console_endpoint_generation(APP_ENDPOINT_CAMERA);
    if (!current) {
        camera_display_end();display_session.token=0;return;
    }
    if (display_session.releasing && camera_display_ready()) {
        camera_display_end();display_session.token=0;
        camera_jpeg_start();
        app_message_t reply={.result=camera_controller_active() ? ESP_OK : ESP_ERR_INVALID_STATE};
        app_console_reply(&display_session.release,&reply);return;
    }
    /* A timed-out acquire does not revoke a reservation. UI still owns its
     * token and must release it, including when the real owner drains late. */
    if (display_session.origin.deadline_us<=esp_timer_get_time()) return;
    if (!display_session.ready && camera_display_ready()) {
        app_message_t reply={.result=ESP_OK,.payload.command={.token=display_session.token,.flag=display_session.resume}};
        display_session.ready=true;app_console_reply(&display_session.origin,&reply);
    }
}
static esp_err_t display_request(app_message_t *message,app_message_t *reply,bool *deferred)
{
    if (message->source!=APP_ENDPOINT_UI || message->flags!=APP_MESSAGE_REQUEST || !message->generation ||
        !message->payload.command.token || message->payload.command.index>1 ||
        message->generation!=app_console_endpoint_generation(APP_ENDPOINT_UI) ||
        message->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_CAMERA)) return ESP_ERR_INVALID_ARG;
    if (message->payload.command.index) {
        if (display_session.token) return ESP_ERR_INVALID_STATE;
        camera_debug_status_t status;camera_debug_get_status(&status);
        if (!camera_display_begin()) return ESP_ERR_INVALID_STATE;
        display_session.origin=*message;display_session.token=message->payload.command.token;
        display_session.ready=display_session.releasing=false;display_session.resume=status.busy && !status.stopped;
        *deferred=true;return ESP_OK;
    }
    if (!display_session.token) return ESP_ERR_NOT_FOUND;
    if (display_session.token!=message->payload.command.token || display_session.origin.generation!=message->generation)
        return ESP_ERR_INVALID_STATE;
    bool resume=display_session.resume && message->payload.command.flag;
    if (!display_session.ready) {
        app_message_t cancelled={.result=ESP_ERR_INVALID_STATE};app_console_reply(&display_session.origin,&cancelled);
    }
    if (resume && !camera_display_ready()) {
        if (display_session.releasing) {
            app_message_t replaced={.result=ESP_ERR_INVALID_STATE};app_console_reply(&display_session.release,&replaced);
        }
        display_session.release=*message;display_session.releasing=true;*deferred=true;return ESP_OK;
    }
    camera_display_end();display_session.token=0;
    if (resume) camera_jpeg_start();
    reply->payload.command.token=message->payload.command.token;
    return !resume || camera_controller_active() ? ESP_OK : ESP_ERR_INVALID_STATE;
}
#endif

static void finish_stops(void)
{
    for (unsigned i=0;i<stop_count;) {
        bool expired=stops[i].deadline_us<=esp_timer_get_time();
        if (!expired && camera_controller_active() && stop_lifetimes[i]==camera_controller_lifetime()) { ++i; continue; }
        if (!expired) {
            app_message_t reply={.result=ESP_OK};
            app_console_reply(&stops[i],&reply);
        }
        app_message_release(&stops[i]);
        stops[i]=stops[--stop_count];
        stop_lifetimes[i]=stop_lifetimes[stop_count];
    }
}
static esp_err_t handle(app_message_t *message,app_message_t *reply,bool *deferred)
{
    if (message->lease || (message->flags&(APP_MESSAGE_REPLY|APP_MESSAGE_BULK)))
        return ESP_ERR_INVALID_ARG;
    if ((message->flags&APP_MESSAGE_REQUEST) && message->deadline_us<=esp_timer_get_time())
        return ESP_ERR_TIMEOUT;
    if (message->type==APP_MESSAGE_UI_FRAME_RESULT) {
        if (message->source!=APP_ENDPOINT_UI || !message->generation ||
            !message->payload.command.token || (message->flags&APP_MESSAGE_REQUEST))
            return ESP_ERR_INVALID_ARG;
        /* Two slots mean at most two outstanding authentic UI completions.
         * Preserve metadata independently of the owner's blocked network IO. */
        if (!camera_controller_active() || message->generation!=camera_controller_frame_generation()) return ESP_ERR_INVALID_STATE;
        return xQueueSend(results,message,0)==pdTRUE ? ESP_OK : ESP_ERR_NO_MEM;
    }
    switch (message->type) {
    case APP_MESSAGE_CAMERA_DISPLAY_SESSION:
#if CONFIG_REMOTE_DBG_SIM
        return display_request(message,reply,deferred);
#else
        return ESP_ERR_NOT_SUPPORTED;
#endif
    case APP_MESSAGE_WIFI_NETWORK_CHANGED:
        if (message->source!=APP_ENDPOINT_WIFI || !message->payload.network.generation) return ESP_ERR_INVALID_ARG;
        camera_controller_network_changed(message->payload.network.generation); return ESP_OK;
    case APP_MESSAGE_CAMERA_STOP: {
        bool admission_only=message->payload.command.flag;
        if (admission_only && (message->source!=APP_ENDPOINT_UART || message->flags!=APP_MESSAGE_REQUEST))
            return ESP_ERR_INVALID_ARG;
        uint32_t lifetime=camera_controller_lifetime();
        camera_stop_request();
        if (!admission_only && (message->flags&APP_MESSAGE_REQUEST) && camera_controller_active() && lifetime==camera_controller_lifetime()) {
            if (stop_count==APP_CONSOLE_PENDING_CAPACITY) return ESP_ERR_NO_MEM;
            stop_lifetimes[stop_count]=lifetime;
            stops[stop_count++]=*message; *deferred=true;
        }
        return ESP_OK;
    }
    case APP_MESSAGE_CAMERA_START:
        if (stop_count || camera_controller_active()) return ESP_ERR_INVALID_STATE;
        if (message->payload.command.flag) camera_pair_start(); else camera_jpeg_start();
        return camera_controller_active() ? ESP_OK : ESP_ERR_INVALID_STATE;
    case APP_MESSAGE_CAMERA_STATUS: {
        camera_debug_status_t status={0}; camera_debug_get_status(&status);
        reply->payload.camera=(app_camera_status_t){.busy=status.busy,.session=status.session,
            .stopped=status.stopped,.last_io=status.last_io};
        for (unsigned i=0;i<sizeof status.phase;++i) reply->payload.camera.phase[i]=status.phase[i];
        return ESP_OK;
    }
    case APP_MESSAGE_CAMERA_CAPABILITIES:
        camera_gamepad_caps(&reply->payload.capabilities); return ESP_OK;
    case APP_MESSAGE_CAMERA_ACTION:
        {
            bool accepted=camera_gamepad_action(message->payload.action);
            camera_gamepad_caps(&reply->payload.capabilities);
            return accepted ? ESP_OK : ESP_ERR_INVALID_STATE;
        }
    case APP_MESSAGE_CAMERA_MENU_ACTION:
    case APP_MESSAGE_CAMERA_SETTING_ADJUST:
        if (!camera_controller_admitting()) return ESP_ERR_INVALID_STATE;
        if (message->payload.command.index>=APP_CAMERA_PROPERTY_BATTERY ||
            (message->payload.command.direction!=1 && message->payload.command.direction!=-1))
            return ESP_ERR_INVALID_ARG;
        if (!camera_controller_active()) return ESP_ERR_INVALID_STATE;
        {
            gamepad_caps_t caps; camera_gamepad_caps(&caps);
            if (!caps.session || (message->payload.command.token && message->payload.command.token!=caps.generation))
                return ESP_ERR_INVALID_STATE;
        }
        camera_controller_setting(message->payload.command.index,message->payload.command.direction);
        return ESP_OK;
    default: return ESP_ERR_NOT_SUPPORTED;
    }
}
static void endpoint_task(void *context)
{
    (void)context;
    for (;;) {
        finish_stops();
#if CONFIG_REMOTE_DBG_SIM
        finish_display();
#endif
        app_message_t message={0},reply={0}; bool deferred=false;
        esp_err_t error=app_console_receive(APP_ENDPOINT_CAMERA,&message,25);
        if (error==ESP_ERR_INVALID_STATE) break;
        if (error!=ESP_OK) continue;
        reply.result=handle(&message,&reply,&deferred);
        if (!deferred && (message.flags&APP_MESSAGE_REQUEST)) app_console_reply(&message,&reply);
        app_message_release(&message);
    }
    for (unsigned i=0;i<stop_count;++i) app_message_release(&stops[i]);
    stop_count=0;
#if CONFIG_REMOTE_DBG_SIM
    if (display_session.token) { camera_display_end();display_session.token=0; }
#endif
    atomic_store(&endpoint_done,true);
    vTaskDelete(NULL);
}
esp_err_t camera_endpoint_start(void)
{
    if (results) return ESP_ERR_INVALID_STATE;
    app_console_status_t router; app_console_get_status(&router);
    if (!router.running || !router.accepting) return ESP_ERR_INVALID_STATE;
    if (!router.subscriptions_frozen) network_subscribed=false;
    results=xQueueCreate(4,sizeof(app_message_t));
    if (!results) return ESP_ERR_NO_MEM;
    const app_endpoint_config_t config={16,2};
    esp_err_t error=app_console_endpoint_register(APP_ENDPOINT_CAMERA,&config);
    bool registered=error==ESP_OK;
    if (error==ESP_OK && !network_subscribed) {
        error=app_console_subscribe(APP_MESSAGE_WIFI_NETWORK_CHANGED,APP_ENDPOINT_CAMERA);
        if (error==ESP_OK) network_subscribed=true;
    }
    atomic_store(&endpoint_done,false);
    if (error==ESP_OK && xTaskCreate(endpoint_task,"camera_endpoint",4096,NULL,4,NULL)!=pdPASS)
        error=ESP_ERR_NO_MEM;
    if (error!=ESP_OK) {
        if (registered) app_console_endpoint_stop(APP_ENDPOINT_CAMERA);
        vQueueDelete(results); results=NULL;
    }
    return error;
}
bool camera_endpoint_quiesce(uint32_t timeout_ms)
{
    if (!results) return true;
    app_console_endpoint_stop(APP_ENDPOINT_CAMERA);
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while (!atomic_load(&endpoint_done)) {
        if (esp_timer_get_time()>=deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    app_message_t message;
    while (xQueueReceive(results,&message,0)==pdTRUE) app_message_release(&message);
    vQueueDelete(results); results=NULL;
    return true;
}
void camera_endpoint_stop(void) { (void)camera_endpoint_quiesce(UINT32_MAX); }
bool camera_endpoint_frame_result(app_message_t *message)
{
    return results && message && xQueueReceive(results,message,0)==pdTRUE;
}
