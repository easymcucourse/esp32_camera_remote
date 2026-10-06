#include "ui_menu_messages.h"
#include "app_ui_internal.h"
#include "ui_model.h"
#include <assert.h>
#include <stdio.h>
static int64_t now=100;
static esp_err_t mode_result;
int64_t esp_timer_get_time(void) { return now; }
void app_ui_refresh_wifi_info(void) {}
uint32_t app_console_endpoint_generation(app_endpoint_t endpoint)
{ return endpoint==APP_ENDPOINT_UI ? 3 : 1; }
static app_message_t request={.type=APP_MESSAGE_UI_MENU_ACTION,.source=APP_ENDPOINT_INPUT,
    .target=APP_ENDPOINT_UI,.flags=APP_MESSAGE_REQUEST,.generation=1,.endpoint_epoch=3,.deadline_us=1000};
static app_message_t reply;
static app_ui_menu_result_t act(pad_action_type_t type,int value)
{
    request.payload.action=(pad_action_t){type,value,17};
    assert(ui_menu_message_apply(&request,&reply)==ESP_OK);
    return reply.payload.menu;
}
int main(void)
{
    assert(act(PAD_ACTION_MENU_MOVE,1).state.selected==5);
    assert(act(PAD_ACTION_MENU_STEP,1).route==APP_UI_MENU_HANDLED);
    assert(act(PAD_ACTION_UI_TOGGLE,0).state.settings);
    const unsigned rows[]={0,1,2,3,4,6,9,7,5};
    for (unsigned i=0;i<9;++i) {
        app_ui_menu_result_t r=act(PAD_ACTION_MENU_MOVE,1);
        assert(r.state.selected==rows[i]);
        r=act(PAD_ACTION_MENU_STEP,-1);
        if (rows[i]<7) assert(r.route==APP_UI_MENU_CAMERA && r.property==APP_CAMERA_PROPERTY_SHUTTER+rows[i]);
        else assert(r.route==APP_UI_MENU_HANDLED && !r.state.wifi_menu);
    }
    /* Wi-Fi is information only for both normal command sources. */
    while (act(PAD_ACTION_MENU_MOVE,1).state.selected!=7) {}
    const app_endpoint_t sources[]={APP_ENDPOINT_INPUT,APP_ENDPOINT_UART};
    const pad_action_type_t network_actions[]={PAD_ACTION_MENU_CONFIRM,PAD_ACTION_MENU_STEP,
        PAD_ACTION_MENU_BACK,PAD_ACTION_RELEASE_ALL};
    for(unsigned source=0;source<2;++source){
        request.source=sources[source];
        for(unsigned i=0;i<4;++i){
            app_ui_menu_result_t r=act(network_actions[i],network_actions[i]==PAD_ACTION_MENU_STEP?1:0);
            assert(r.route==APP_UI_MENU_HANDLED && r.state.selected==7 && r.state.settings);
            assert(!r.state.wifi_menu && !r.state.extra_menu);
        }
    }
    request.source=APP_ENDPOINT_INPUT;
    /* MORE follows METER. CONFIRM opens it; its nine rows map to semantic IDs. */
    while (act(PAD_ACTION_MENU_MOVE,1).state.selected!=9) {}
    assert(act(PAD_ACTION_MENU_CONFIRM,0).state.extra_menu);
    for (unsigned i=0;i<9;++i) {
        app_ui_menu_result_t r=act(PAD_ACTION_MENU_STEP,1);
        assert(r.route==APP_UI_MENU_CAMERA && r.property==APP_CAMERA_PROPERTY_ASPECT+i);
        assert(act(PAD_ACTION_MENU_CONFIRM,0).state.extra_menu);
        act(PAD_ACTION_MENU_MOVE,1);
    }
    assert(act(PAD_ACTION_MENU_STEP,1).route==APP_UI_MENU_HANDLED); /* EXIT */
    assert(!act(PAD_ACTION_MENU_CONFIRM,0).state.extra_menu);
    act(PAD_ACTION_MENU_CONFIRM,0);
    assert(!act(PAD_ACTION_MENU_BACK,0).state.extra_menu);
    act(PAD_ACTION_MENU_CONFIRM,0);
    assert(!act(PAD_ACTION_UI_TOGGLE,0).state.settings && !app_ui_extra_menu_active());
    act(PAD_ACTION_UI_TOGGLE,0);
    assert(act(PAD_ACTION_MENU_MOVE,1).route==APP_UI_MENU_HANDLED && !reply.payload.menu.state.wifi_menu);
    assert(!act(PAD_ACTION_UI_TOGGLE,0).state.settings);
    unsigned selected=app_ui_menu_selected();
    request.payload.action=(pad_action_t){PAD_ACTION_MENU_MOVE,0,17};
    assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);
    request.payload.action=(pad_action_t){PAD_ACTION_UI_TOGGLE,0,17};
    now=1000;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_TIMEOUT && !app_ui_settings_mode());now=100;
    request.flags=0;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);request.flags=APP_MESSAGE_REQUEST;
    request.source=APP_ENDPOINT_CAMERA;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);
    request.source=APP_ENDPOINT_INPUT;request.generation=0;
    assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);request.generation=1;
    request.generation=2;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_STATE);request.generation=1;
    request.endpoint_epoch=2;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_STATE);request.endpoint_epoch=3;
    request.lease=(app_message_lease_t *)&reply;
    assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);request.lease=NULL;
    request.payload.action.type=PAD_ACTION_S1;assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_ARG);
    assert(app_ui_menu_selected()==selected && !app_ui_settings_mode());
    app_message_t property={.type=APP_MESSAGE_UI_PROPERTY_STATUS,.source=APP_ENDPOINT_UART,.target=APP_ENDPOINT_UI,
        .flags=APP_MESSAGE_REQUEST,.generation=1,.endpoint_epoch=3,.deadline_us=1000,
        .payload.command={.index=APP_CAMERA_PROPERTY_ASPECT}};
    assert(ui_property_message(&property,&reply)==ESP_OK && reply.payload.property.property==APP_CAMERA_PROPERTY_ASPECT);
    property.payload.command.index=APP_CAMERA_PROPERTY_EV;
    assert(ui_property_message(&property,&reply)==ESP_ERR_INVALID_ARG);
    property.payload.command.index=APP_CAMERA_PROPERTY_WB_GM;
    assert(ui_property_message(&property,&reply)==ESP_OK && reply.payload.property.property==APP_CAMERA_PROPERTY_WB_GM);
    property.generation=2;assert(ui_property_message(&property,&reply)==ESP_ERR_INVALID_STATE);property.generation=1;
    property.endpoint_epoch=2;assert(ui_property_message(&property,&reply)==ESP_ERR_INVALID_STATE);property.endpoint_epoch=3;
    property.source=APP_ENDPOINT_CAMERA;assert(ui_property_message(&property,&reply)==ESP_ERR_INVALID_ARG);property.source=APP_ENDPOINT_UART;
    property.deadline_us=100;assert(ui_property_message(&property,&reply)==ESP_ERR_TIMEOUT);
    mode_result=ESP_ERR_INVALID_STATE;bool previous=app_ui_settings_mode();
    request.payload.action.type=PAD_ACTION_UI_TOGGLE;
    assert(ui_menu_message_apply(&request,&reply)==ESP_ERR_INVALID_STATE && app_ui_settings_mode()==previous);
    puts("UI menu messages preserve navigation and resolve semantic parameters");
}

esp_err_t ui_mode_enter_normal(unsigned reason) { assert(reason==APP_NORMAL_SETTINGS);return mode_result; }
