#include "ui_input_messages.h"
#include "app_ui_internal.h"
#include <limits.h>

static uint32_t generation;
esp_err_t ui_input_message_apply(const app_message_t *m)
{
    if (!m || m->type!=APP_MESSAGE_INPUT_STATE || m->source!=APP_ENDPOINT_INPUT ||
        m->flags!=APP_MESSAGE_EVENT || m->lease || !m->generation)
        return ESP_ERR_INVALID_ARG;
    const app_input_state_t *s=&m->payload.input;
    if (s->kind>1 || s->gimbal>3 || (s->battery>10 && s->battery!=255) ||
        s->rx<INT8_MIN || s->rx>INT8_MAX || s->ry<INT8_MIN || s->ry>INT8_MAX ||
        s->buttons>0x3ffffu || (s->connected && ((!s->atom_online && !s->sim) || s->mismatch)))
        return ESP_ERR_INVALID_ARG;
    if (m->generation<generation) return ESP_ERR_INVALID_STATE;
    generation=m->generation;
    app_ui_set_sim(s->sim);
    app_ui_set_atom_status(s->atom_online,s->connected);
    app_ui_set_atom_protocol(s->mismatch,s->gimbal,s->atom_online && !s->mismatch && s->gimbal_fault);
    app_ui_set_controller_battery(s->connected ? s->battery : 255,s->kind==1);
    return ESP_OK;
}
