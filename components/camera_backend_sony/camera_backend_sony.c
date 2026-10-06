#include "private/camera_backend_sony_factory.h"
#include "ptpip_protocol.h"
#include "ptp_codes.h"
#include "ptp_dataset.h"
#include "sony_control_encoder.h"
#include "sony_codes.h"
#include "sony_props.h"
#include "sony_liveview.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool seen;
    uint16_t type;
    camera_property_t property;
    camera_value_t choices[64];
} property_t;
typedef struct {
    camera_backend_t interface;
    ptpip_client_t ptp;
    bool initialized, duplicate;
    property_t properties[CAMERA_SETTING_COUNT];
    camera_capabilities_t capabilities;
} sony_backend_t;
/* Bind the pure encoder to this backend's sole PTP instance. No second
 * control facade or copied session/transaction/deadline state. */
static bool control_write(void *context, uint16_t opcode, uint16_t property,
    const uint8_t *data, size_t size, bool *accepted)
{ return ptpip_client_send_data(context, opcode, property, data, size, accepted); }
static const uint16_t codes[CAMERA_SETTING_COUNT] = {
    SONY_DPC_EXPOSURE_PROGRAM, SONY_DPC_SHUTTER_SPEED, SONY_DPC_F_NUMBER,
    SONY_DPC_ISO, SONY_DPC_EXPOSURE_BIAS, SONY_DPC_WHITE_BALANCE,
    SONY_DPC_FOCUS_MODE, SONY_DPC_METERING, SONY_DPC_ASPECT_RATIO,
    SONY_DPC_DRIVE_MODE, SONY_DPC_PICTURE_EFFECT, SONY_DPC_DRO,
    SONY_DPC_FOCUS_AREA, SONY_DPC_WIRELESS_FLASH, SONY_DPC_WB_TEMPERATURE,
    SONY_DPC_WB_AB, SONY_DPC_WB_GM, 0xd218, SONY_DPC_ZOOM_ENABLE_STATUS,
    SONY_DPC_MOVIE_RECORDING_STATE, SONY_DPC_FLASH
};
static camera_value_type_t value_type(uint16_t type)
{
    static const camera_value_type_t types[] = {CAMERA_VALUE_I8, CAMERA_VALUE_U8,
        CAMERA_VALUE_I16, CAMERA_VALUE_U16, CAMERA_VALUE_I32, CAMERA_VALUE_U32};
    return types[type - 1];
}
static camera_backend_result_t io_error(sony_backend_t *s)
{
    switch (s->ptp.last_result) {
    case PTPIP_CLIENT_CANCELLED: return CAMERA_BACKEND_CANCELLED;
    case PTPIP_CLIENT_TIMEOUT: return CAMERA_BACKEND_TIMEOUT;
    case PTPIP_CLIENT_NETWORK: return CAMERA_BACKEND_NETWORK;
    case PTPIP_CLIENT_INVALID: return CAMERA_BACKEND_INVALID;
    case PTPIP_CLIENT_INIT_REJECTED: return CAMERA_BACKEND_IDENTITY;
    case PTPIP_CLIENT_OK:
        return s->ptp.response && s->ptp.response != PTP_RC_OK ? CAMERA_BACKEND_REFUSED : CAMERA_BACKEND_PROTOCOL;
    default: return CAMERA_BACKEND_PROTOCOL;
    }
}
static camera_backend_result_t data_result(sony_backend_t *s, ptp_data_status_t result)
{
    if (result == PTP_DATA_OK) return CAMERA_BACKEND_OK;
    if (result == PTP_DATA_REFUSED) return CAMERA_BACKEND_REFUSED;
    return io_error(s);
}
static bool ready(sony_backend_t *s) { return s->initialized && s->ptp.session; }
static bool scope(sony_backend_t *s, uint32_t timeout)
{
    ptpip_client_timeout_set(&s->ptp, PTPIP_CHANNEL_COMMAND, timeout);
    ptpip_client_timeout_set(&s->ptp, PTPIP_CHANNEL_EVENT, timeout);
    return ptpip_client_scope_begin(&s->ptp, timeout);
}
static void collect(void *context, const sony_property_desc_t *desc)
{
    sony_backend_t *s = context;
    for (unsigned i = 0; i < CAMERA_SETTING_COUNT; ++i) {
        if (desc->code != codes[i]) continue;
        property_t *p = &s->properties[i];
        if (p->seen) { s->duplicate = true; return; }
        p->seen = true;
        if (!desc->scalar || desc->type < 1 || desc->type > 6) return;
        static const uint16_t primary[] = {6, 6, 4, 6, 3, 4, 4, 4};
        if (i < sizeof(primary) / sizeof(primary[0]) && desc->type != primary[i]) return;
        p->type = desc->type;
        p->property = (camera_property_t){.setting = i,
            .current = {value_type(desc->type), desc->value}, .writable = desc->writable};
        if (i >= CAMERA_SETTING_BATTERY) p->property.writable = false;
        if (desc->form == 2) {
            if (desc->choice_count > 64) p->property.writable = false;
            else {
                p->property.choice_count = desc->choice_count; p->property.choices = p->choices;
                for (unsigned j = 0; j < desc->choice_count; ++j) {
                    uint32_t bits = 0; sony_descriptor_choice(desc, j, &bits);
                    p->choices[j] = (camera_value_t){value_type(desc->type), bits};
                }
                if (i == CAMERA_SETTING_EV) for (unsigned j = 1; j < p->property.choice_count; ++j) {
                    camera_value_t value = p->choices[j]; unsigned k = j;
                    int32_t signed_value = value.bits <= INT16_MAX ? (int32_t)value.bits : (int32_t)value.bits - 65536;
                    while (k) {
                        uint32_t bits = p->choices[k - 1].bits;
                        int32_t prior = bits <= INT16_MAX ? (int32_t)bits : (int32_t)bits - 65536;
                        if (prior <= signed_value) break;
                        p->choices[k] = p->choices[k - 1]; --k;
                    }
                    p->choices[k] = value;
                }
            }
        }
        if (i == CAMERA_SETTING_MODE && !p->property.choice_count) p->property.writable = false;
        p->property.relative = (i == CAMERA_SETTING_SHUTTER || i == CAMERA_SETTING_APERTURE) &&
            !p->property.choice_count;
        if (i == CAMERA_SETTING_SHUTTER && (desc->value == UINT32_MAX ||
            (desc->value && !(desc->value & 0xffff)))) p->property.writable = false;
        if (i == CAMERA_SETTING_APERTURE && (!desc->value || desc->value >= 0xfffd))
            p->property.writable = false;
        return;
    }
}
static bool snapshot(sony_backend_t *s, const uint8_t *data, size_t size)
{
    memset(s->properties, 0, sizeof s->properties); s->duplicate = false;
    s->capabilities = (camera_capabilities_t){0};
    sony_focus_caps_t caps;
    if (!sony_parse_descriptors(data, size, collect, s) || s->duplicate ||
        !sony_parse_focus_caps(data, size, &caps)) {
        memset(s->properties, 0, sizeof s->properties); return false;
    }
    s->capabilities = (camera_capabilities_t){caps.focus_known,
        caps.focus_mode == SONY_FOCUS_MODE_MANUAL, caps.zoom_known, caps.zoom_enabled == 1,
        caps.recording_known, caps.recording};
    return true;
}
static camera_backend_result_t connect_camera(void *context, const camera_connection_t *c,
    void *scratch, size_t capacity, uint32_t timeout, camera_peer_t *peer)
{
    sony_backend_t *s = context;
    if (!c || !scratch || !capacity || !peer || !timeout || !c->port ||
        !c->handshake_timeout_ms || !c->address[0] || !memchr(c->address, 0, sizeof c->address) ||
        !memchr(c->local_name, 0, sizeof c->local_name)) return CAMERA_BACKEND_INVALID;
    if (s->ptp.channels[0].token || s->ptp.channels[1].token || s->initialized) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    camera_peer_t found = {0}; camera_backend_result_t result = CAMERA_BACKEND_OK;
    bool ok = ptpip_client_open(&s->ptp, PTPIP_CHANNEL_COMMAND, c->address, c->port, timeout) &&
        ptpip_client_initialize_command(&s->ptp, c->local_guid, c->local_name, c->handshake_timeout_ms);
    if (ok && c->require_peer_guid && memcmp(c->peer_guid, s->ptp.peer_guid, sizeof c->peer_guid)) {
        ptpip_client_transaction_end(&s->ptp); return CAMERA_BACKEND_IDENTITY;
    }
    if (ok) ok = ptpip_client_open(&s->ptp, PTPIP_CHANNEL_EVENT, c->address, c->port, timeout) &&
        ptpip_client_initialize_event(&s->ptp);
    ptpip_client_timeout_set(&s->ptp, PTPIP_CHANNEL_COMMAND, 5000);
    if (ok) ok = ptpip_client_operation(&s->ptp, PTP_OC_OPEN_SESSION, true);
    if (!ok) result = io_error(s);
    const struct { uint16_t opcode; unsigned count; uint32_t params[3]; } steps[] = {
        {SONY_OC_SDIO_CONNECT, 3, {1,0,0}}, {SONY_OC_SDIO_CONNECT, 3, {2,0,0}},
        {PTP_OC_GET_DEVICE_INFO, 1, {0}}, {SONY_OC_SDIO_GET_EXT_DEVICE_INFO, 1, {300}},
        {SONY_OC_SDIO_CONNECT, 3, {3,0,0}}, {SONY_OC_SDIO_GET_EXT_DEVICE_INFO, 1, {300}},
        {SONY_OC_GET_ALL_EXT_PROP_INFO, 1, {0}}, {PTP_OC_GET_OBJECT_INFO, 1, {SONY_LIVEVIEW_HANDLE}}
    };
    for (unsigned i = 0; ok && i < sizeof steps / sizeof steps[0]; ++i) {
        size_t size = 0; uint16_t response = 0;
        ptp_data_status_t data = ptpip_client_request_data(&s->ptp, steps[i].opcode,
            steps[i].params, steps[i].count, scratch, capacity, &size, &response);
        result = data_result(s, data);
        if (steps[i].opcode == PTP_OC_GET_OBJECT_INFO && data == PTP_DATA_REFUSED &&
            (response == 0x2009 || response == PTP_RC_ACCESS_DENIED)) result = CAMERA_BACKEND_OK;
        if (result == CAMERA_BACKEND_OK && steps[i].opcode == PTP_OC_GET_DEVICE_INFO &&
            !ptp_parse_device_info(scratch, size, found.model, found.firmware)) result = CAMERA_BACKEND_PROTOCOL;
        if (result == CAMERA_BACKEND_OK && steps[i].opcode == SONY_OC_GET_ALL_EXT_PROP_INFO &&
            !snapshot(s, scratch, size)) result = CAMERA_BACKEND_PROTOCOL;
        ok = result == CAMERA_BACKEND_OK;
    }
    if (ok) {
        memcpy(found.guid, s->ptp.peer_guid, sizeof found.guid);
        memcpy(found.name, s->ptp.peer_name, sizeof found.name);
        *peer = found; s->initialized = true;
    }
    ptpip_client_transaction_end(&s->ptp);
    /* Failure intentionally retains channel tokens. Owner must disconnect before
     * retry/destroy; do not hide a cleanup failure or invent another session. */
    return result;
}
static camera_backend_result_t disconnect_camera(void *context, uint32_t timeout)
{
    sony_backend_t *s = context;
    if (!timeout || s->ptp.transaction_depth) return CAMERA_BACKEND_INVALID;
    bool orderly = ready(s) && s->ptp.last_result == PTPIP_CLIENT_OK;
    s->initialized = false; memset(s->properties, 0, sizeof s->properties);
    s->capabilities = (camera_capabilities_t){0};
    /* CLOSE must work after cancel/network failure. A cancelled client cannot
     * begin a normal scope; token release deliberately bypasses that predicate. */
    if (!ptpip_client_cleanup_begin(&s->ptp, timeout)) return CAMERA_BACKEND_STATE;
    /* A complete, healthy session ends normally. Broken/unconsumed I/O phases
     * are never followed by a protocol command; channel cleanup still runs. */
    if (orderly) ptpip_client_operation(&s->ptp, PTP_OC_CLOSE_SESSION, false);
    bool event = ptpip_client_close(&s->ptp, PTPIP_CHANNEL_EVENT, timeout);
    bool command = ptpip_client_close(&s->ptp, PTPIP_CHANNEL_COMMAND, timeout);
    ptpip_client_transaction_end(&s->ptp);
    return event && command ? CAMERA_BACKEND_OK : io_error(s);
}
static void cancel_camera(void *context) { sony_backend_t *s = context; ptpip_client_cancel(&s->ptp); }
static void network_camera(void *context, uint32_t generation)
{ sony_backend_t *s = context; ptpip_client_network_changed(&s->ptp, generation); }
static camera_backend_result_t destroy_camera(void *context)
{
    sony_backend_t *s = context;
    if (s->ptp.transaction_depth || s->ptp.channels[0].token || s->ptp.channels[1].token)
        return CAMERA_BACKEND_STATE;
    free(s); return CAMERA_BACKEND_OK;
}
static camera_backend_result_t properties(void *context, void *scratch, size_t capacity,
    uint32_t timeout, camera_property_visitor_t visitor, void *user, camera_capabilities_t *caps)
{
    sony_backend_t *s = context;
    if (!scratch || !capacity || !timeout || !caps) return CAMERA_BACKEND_INVALID;
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    uint32_t group = 0; size_t size = 0; uint16_t response = 0;
    camera_backend_result_t result = data_result(s, ptpip_client_request_data(&s->ptp,
        SONY_OC_GET_ALL_EXT_PROP_INFO, &group, 1, scratch, capacity, &size, &response));
    if (result == CAMERA_BACKEND_OK && !snapshot(s, scratch, size)) result = CAMERA_BACKEND_PROTOCOL;
    ptpip_client_transaction_end(&s->ptp);
    if (result == CAMERA_BACKEND_OK) {
        *caps = s->capabilities;
        if (visitor) for (unsigned i = 0; i < CAMERA_SETTING_COUNT; ++i)
            if (s->properties[i].type) visitor(user, &s->properties[i].property);
    } else { memset(s->properties, 0, sizeof s->properties); s->capabilities = (camera_capabilities_t){0}; }
    return result;
}
static camera_backend_result_t write_result(sony_backend_t *s, bool ok, bool accepted)
{ return !ok ? io_error(s) : accepted ? CAMERA_BACKEND_OK : CAMERA_BACKEND_REFUSED; }
static camera_backend_result_t set(void *context, camera_setting_t setting, camera_value_t value, uint32_t timeout)
{
    sony_backend_t *s = context;
    if ((unsigned)setting >= CAMERA_SETTING_COUNT || !timeout) return CAMERA_BACKEND_INVALID;
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    property_t *p = &s->properties[setting];
    if (!p->type || !p->property.writable || p->property.relative) return CAMERA_BACKEND_UNSUPPORTED;
    if (value.type != p->property.current.type || (p->type <= 2 && value.bits > UINT8_MAX) ||
        (p->type <= 4 && value.bits > UINT16_MAX)) return CAMERA_BACKEND_INVALID;
    if (p->property.choice_count) {
        bool offered = false;
        for (unsigned i = 0; i < p->property.choice_count; ++i)
            if (p->choices[i].bits == value.bits) offered = true;
        if (!offered) return CAMERA_BACKEND_INVALID;
    }
    if (!scope(s, timeout)) return io_error(s);
    bool accepted = false;
    const sony_control_writer_t writer = {&s->ptp, control_write};
    bool ok = sony_encode_set_scalar(&writer, codes[setting], p->type, value.bits, &accepted);
    ptpip_client_transaction_end(&s->ptp); return write_result(s, ok, accepted);
}
static camera_backend_result_t step(void *context, camera_setting_t setting, int direction, uint32_t timeout)
{
    sony_backend_t *s = context;
    if ((unsigned)setting >= CAMERA_SETTING_COUNT || !timeout || (direction != -1 && direction != 1))
        return CAMERA_BACKEND_INVALID;
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    if (!s->properties[setting].property.writable || !s->properties[setting].property.relative)
        return CAMERA_BACKEND_UNSUPPORTED;
    if (!scope(s, timeout)) return io_error(s);
    bool accepted = false;
    const sony_control_writer_t writer = {&s->ptp, control_write};
    bool ok = sony_encode_setting_step(&writer, codes[setting], direction, &accepted);
    ptpip_client_transaction_end(&s->ptp); return write_result(s, ok, accepted);
}
static camera_backend_result_t liveview(void *context, void *scratch, size_t capacity,
    uint32_t timeout, camera_frame_t *frame)
{
    sony_backend_t *s = context;
    if (!scratch || !capacity || !timeout || !frame) return CAMERA_BACKEND_INVALID;
    *frame = (camera_frame_t){0};
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    uint32_t handle = SONY_LIVEVIEW_HANDLE; size_t size = 0; uint16_t response = 0;
    camera_backend_result_t result = data_result(s, ptpip_client_request_data(&s->ptp,
        PTP_OC_GET_OBJECT, &handle, 1, scratch, capacity, &size, &response));
    if (result == CAMERA_BACKEND_REFUSED && response == PTP_RC_ACCESS_DENIED)
        result = CAMERA_BACKEND_NOT_READY;
    ptpip_client_transaction_end(&s->ptp);
    sony_liveview_t view;
    if (result == CAMERA_BACKEND_OK) {
        if (!sony_liveview_parse(scratch, size, &view)) result = CAMERA_BACKEND_DROPPED;
        else *frame = (camera_frame_t){view.jpeg, view.jpeg_size};
    }
    return result;
}
static camera_backend_result_t action(void *context, camera_action_t action, int value, uint32_t timeout)
{
    sony_backend_t *s = context;
    if (!timeout || (unsigned)action > CAMERA_ACTION_ZOOM ||
        (action == CAMERA_ACTION_FOCUS_STEP ? (value != -1 && value != 1) :
        action == CAMERA_ACTION_ZOOM ? (value < -1 || value > 1) : (value != 0 && value != 1)))
        return CAMERA_BACKEND_INVALID;
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    bool accepted = false, ok = false;
    const sony_control_writer_t writer = {&s->ptp, control_write};
    switch (action) {
    case CAMERA_ACTION_FOCUS_STEP: ok = sony_encode_manual_focus_step(&writer, value, &accepted); break;
    case CAMERA_ACTION_SHUTTER_HALF: case CAMERA_ACTION_SHUTTER_FULL:
        ok = sony_encode_shutter_button(&writer, action == CAMERA_ACTION_SHUTTER_FULL, value, &accepted); break;
    case CAMERA_ACTION_RECORD: ok = sony_encode_movie_record(&writer, value, &accepted); break;
    case CAMERA_ACTION_ZOOM: ok = sony_encode_zoom(&writer, value, &accepted); break;
    }
    ptpip_client_transaction_end(&s->ptp); return write_result(s, ok, accepted);
}
static camera_backend_result_t events(void *context, uint32_t timeout, bool *changed)
{
    sony_backend_t *s = context;
    if (!timeout || !changed) return CAMERA_BACKEND_INVALID;
    *changed = false;
    if (!ready(s)) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    camera_backend_result_t result = CAMERA_BACKEND_OK;
    for (unsigned i = 0; i < 32; ++i) {
        ptpip_event_t event; bool available;
        if (!ptpip_client_next_event(&s->ptp, &event, &available)) { result = io_error(s); break; }
        if (!available) break;
        if (!event.probe && event.code == 0xc203) *changed = true;
    }
    ptpip_client_transaction_end(&s->ptp);
    if (result != CAMERA_BACKEND_OK) *changed = false;
    return result;
}
static camera_backend_result_t probe(void *context, const char *address, uint16_t port, uint32_t timeout)
{
    sony_backend_t *s = context;
    if (!address || !*address || strlen(address) >= 16 || !port || !timeout) return CAMERA_BACKEND_INVALID;
    if (s->ptp.channels[0].token || s->ptp.channels[1].token || s->initialized) return CAMERA_BACKEND_STATE;
    if (!scope(s, timeout)) return io_error(s);
    bool ok = ptpip_client_open(&s->ptp, PTPIP_CHANNEL_COMMAND, address, port, timeout);
    ptpip_client_transaction_end(&s->ptp);
    return ok ? CAMERA_BACKEND_OK : io_error(s);
}
static const camera_backend_ops_t ops = {
    .api_version = CAMERA_BACKEND_API_VERSION,
    .capabilities = CAMERA_BACKEND_CAP_PROPERTIES | CAMERA_BACKEND_CAP_LIVEVIEW |
        CAMERA_BACKEND_CAP_ACTIONS | CAMERA_BACKEND_CAP_EVENTS | CAMERA_BACKEND_CAP_PROBE,
    .connect = connect_camera, .disconnect = disconnect_camera, .cancel = cancel_camera,
    .network_changed = network_camera, .destroy = destroy_camera,
    .properties = properties, .set = set, .step = step, .liveview = liveview,
    .action = action, .events = events, .probe = probe
};
camera_backend_result_t camera_backend_sony_create(uint32_t generation,
    bool (*cancelled)(void *context), void *context, camera_backend_t **out)
{
    if (!out || *out || !generation) return CAMERA_BACKEND_INVALID;
    sony_backend_t *s = calloc(1, sizeof *s);
    if (!s) return CAMERA_BACKEND_NO_MEMORY;
    if (!ptpip_client_init(&s->ptp, APP_ENDPOINT_CAMERA, generation, cancelled, context) ||
        camera_backend_bind(&s->interface, &ops, s, ops.capabilities) != CAMERA_BACKEND_OK) {
        free(s); return CAMERA_BACKEND_INVALID;
    }
    *out = &s->interface; return CAMERA_BACKEND_OK;
}
