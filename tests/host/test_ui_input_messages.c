#include "ui_input_messages.h"
#include "app_ui_internal.h"
#include <assert.h>
#include <stdio.h>
static unsigned changes,battery,gimbal;
static bool atom,connected,sim,mismatch,xbox;
void app_ui_set_sim(bool v) { sim=v; ++changes; }
void app_ui_set_atom_status(bool a,bool c) { atom=a; connected=c; ++changes; }
void app_ui_set_atom_protocol(bool m,unsigned g) { mismatch=m; gimbal=g; ++changes; }
void app_ui_set_controller_battery(unsigned b,bool x) { battery=b; xbox=x; ++changes; }
int main(void)
{
    app_message_t m={.type=APP_MESSAGE_INPUT_STATE,.source=APP_ENDPOINT_INPUT,
        .flags=APP_MESSAGE_EVENT,.generation=3,.payload.input={.atom_online=true,
            .connected=true,.battery=8,.kind=0,.gimbal=3}};
    assert(ui_input_message_apply(&m)==ESP_OK);
    assert(changes==4 && atom && connected && battery==8 && !xbox && !sim && !mismatch && gimbal==3);
    m.payload.input.sim=true;m.payload.input.kind=1;m.payload.input.battery=255;
    assert(ui_input_message_apply(&m)==ESP_OK && sim && xbox && battery==255);
    m.payload.input.atom_online=false;
    assert(ui_input_message_apply(&m)==ESP_OK && !atom && connected && sim);
    m.payload.input.atom_online=true;
    unsigned old=changes;
    m.payload.input.battery=11;assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);
    m.payload.input.battery=8;m.payload.input.rx=128;
    assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);m.payload.input.rx=0;
    m.source=APP_ENDPOINT_INPUT_ATOM;assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);
    m.source=APP_ENDPOINT_INPUT;m.flags|=APP_MESSAGE_REQUEST;
    assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);m.flags=APP_MESSAGE_EVENT;
    m.lease=(app_message_lease_t *)&m;assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);m.lease=NULL;
    m.generation=0;assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);
    m.generation=2;assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_STATE && changes==old);
    m.generation=4;m.payload.input.mismatch=true;
    assert(ui_input_message_apply(&m)==ESP_ERR_INVALID_ARG && changes==old);
    m.payload.input.connected=false;m.payload.input.atom_online=false;
    assert(ui_input_message_apply(&m)==ESP_OK && !connected && !atom && mismatch && battery==255);
    puts("typed input state validation and UI model updates passed");
}
