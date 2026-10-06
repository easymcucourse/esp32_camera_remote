#include "ui_menu_messages.h"
#include "ui_mode_messages.h"
#include "ui_model.h"
#include "app_console.h"
#include "esp_timer.h"

void ui_menu_snapshot(app_ui_state_t *state)
{
    app_ui_status_t status;
    app_ui_get_status(&status);
    *state=(app_ui_state_t){.fps_tenths=status.fps_tenths,.battery=status.battery,
        .focus=status.focus,.ev=status.ev,.settings=status.settings,.failed=status.failed,
        .default_password=status.default_password,.selected=app_ui_menu_selected(),
        .extra_menu=app_ui_extra_menu_active(),.info_level=atomic_load(&ui_model_info_level)};
    state->wifi_menu=false;
}
esp_err_t ui_property_message(const app_message_t *m,app_message_t *reply)
{
    if (!m || !reply || m->type!=APP_MESSAGE_UI_PROPERTY_STATUS || m->source!=APP_ENDPOINT_UART ||
        m->target!=APP_ENDPOINT_UI || m->flags!=APP_MESSAGE_REQUEST || m->lease || !m->generation ||
        m->payload.command.index<APP_CAMERA_PROPERTY_ASPECT || m->payload.command.index>APP_CAMERA_PROPERTY_WB_GM)
        return ESP_ERR_INVALID_ARG;
    if (m->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (m->generation!=app_console_endpoint_generation(APP_ENDPOINT_UART) ||
        m->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI)) return ESP_ERR_INVALID_STATE;
    unsigned property=m->payload.command.index;app_ui_extra_status_t status;
    if (!app_ui_get_extra_status(property-APP_CAMERA_PROPERTY_ASPECT,&status)) return ESP_ERR_INVALID_ARG;
    reply->payload.property=(app_camera_property_state_t){.property=property,.actual=status.actual,
        .target=status.target,.status=status.status,.writable=status.writable,.target_valid=status.target_valid,.actual_valid=true};
    return ESP_OK;
}

esp_err_t ui_menu_message_apply(const app_message_t *m, app_message_t *reply)
{
    if (!m || !reply || m->type!=APP_MESSAGE_UI_MENU_ACTION || m->target!=APP_ENDPOINT_UI || m->lease ||
        (m->flags!=APP_MESSAGE_REQUEST && !(m->flags==0 && m->payload.action.type==PAD_ACTION_RELEASE_ALL)) || !m->generation ||
        (m->source!=APP_ENDPOINT_INPUT && m->source!=APP_ENDPOINT_UART))
        return ESP_ERR_INVALID_ARG;
    pad_action_t a=m->payload.action;
    if (a.type!=PAD_ACTION_UI_TOGGLE && a.type!=PAD_ACTION_MENU_MOVE &&
        a.type!=PAD_ACTION_MENU_STEP && a.type!=PAD_ACTION_MENU_CONFIRM &&
        a.type!=PAD_ACTION_MENU_BACK && a.type!=PAD_ACTION_RELEASE_ALL) return ESP_ERR_INVALID_ARG;
    if ((a.type==PAD_ACTION_MENU_MOVE || a.type==PAD_ACTION_MENU_STEP) &&
        a.value!=1 && a.value!=-1) return ESP_ERR_INVALID_ARG;
    bool safety_cancel=a.type==PAD_ACTION_RELEASE_ALL && m->flags==0 && m->deadline_us==0;
    if (!safety_cancel && m->deadline_us<=esp_timer_get_time()) return ESP_ERR_TIMEOUT;
    if (m->generation!=app_console_endpoint_generation(m->source) ||
        m->endpoint_epoch!=app_console_endpoint_generation(APP_ENDPOINT_UI))
        return ESP_ERR_INVALID_STATE;
    app_ui_menu_result_t result={.route=APP_UI_MENU_HANDLED};
    ui_menu_snapshot(&result.state);
    if (a.type==PAD_ACTION_UI_TOGGLE) {
        esp_err_t mode=ui_mode_enter_normal(APP_NORMAL_SETTINGS);
        if(mode!=ESP_OK)return mode;
        app_ui_toggle_settings_mode();
    }
    else if (result.state.settings) {
        if (result.state.extra_menu) {
            if (a.type==PAD_ACTION_MENU_MOVE) app_ui_extra_menu_move(a.value);
            else if (a.type==PAD_ACTION_MENU_BACK ||
                (a.type==PAD_ACTION_MENU_CONFIRM && app_ui_extra_menu_exit_selected()))
                app_ui_extra_menu_open(false);
            else if (a.type==PAD_ACTION_MENU_STEP && !app_ui_extra_menu_exit_selected()) {
                result.route=APP_UI_MENU_CAMERA;
                result.property=APP_CAMERA_PROPERTY_ASPECT+result.state.selected-7;
            }
        } else if (a.type==PAD_ACTION_MENU_MOVE) app_ui_menu_move(a.value);
        else if (result.state.selected==9 && a.type==PAD_ACTION_MENU_CONFIRM)
            app_ui_extra_menu_open(true);
        else if (result.state.selected==7) result.route=APP_UI_MENU_HANDLED; /* Network information only. */
        else if (result.state.selected<7 && a.type==PAD_ACTION_MENU_STEP) {
            result.route=APP_UI_MENU_CAMERA;
            result.property=APP_CAMERA_PROPERTY_SHUTTER+result.state.selected;
        }
    }
    ui_menu_snapshot(&result.state);
    reply->payload.menu=result;
    return ESP_OK;
}
