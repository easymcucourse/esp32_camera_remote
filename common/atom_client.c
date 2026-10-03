#include "atom_client.h"

atom_request_t atom_client_request(const atom_client_t *c)
{
    return (atom_request_t){c->seq, c->online ? ATOM_CMD_POLL : ATOM_CMD_HELLO,
        c->online ? c->ack_id : 0x0202 | ((uint32_t)c->input_mode << 16)};
}
atom_client_result_t atom_client_failure(atom_client_t *c)
{
    if (c->failures < 3) ++c->failures;
    if (c->failures < 3) return ATOM_CLIENT_RETRY;
    bool hello = !c->online;
    c->online = false; c->ack_id = 0; c->failures = 0;
    c->mismatch = hello;
    return hello ? ATOM_CLIENT_MISMATCH : ATOM_CLIENT_OFFLINE;
}
atom_client_result_t atom_client_response(atom_client_t *c, const uint8_t *bytes,
                                          size_t size, const uint8_t **payload)
{
    if (payload) *payload = NULL;
    atom_request_t request = atom_client_request(c);
    atom_response_t response;
    if (!atom_decode_response(bytes, size, request,
        c->online ? ATOM_POLL_SIZE : ATOM_HELLO_SIZE, &response)) return atom_client_failure(c);
    if (response.version != ATOM_PROTOCOL_VERSION || response.status == ATOM_BAD_VERSION) {
        c->online = false; c->mismatch = true; c->ack_id = 0;
        return ATOM_CLIENT_MISMATCH;
    }
    if (response.status != ATOM_OK) return atom_client_failure(c);
    const uint8_t *p = response.payload;
    uint32_t boot = atom_read_le(p, 4);
    if (!boot) return atom_client_failure(c);
    if (!c->online) {
        uint32_t mask = atom_read_le(p + 8, 3);
        if (p[11] || !p[7] || !(p[6] & ATOM_FEATURE_INPUT_MODE) || !(p[6] & 8) || (mask & ~ATOM_BUTTON_MASK))
            return atom_client_failure(c);
        c->boot_id = boot; c->local_mask = mask; c->ack_id = 0;
        c->online = true; c->failures = 0; c->mismatch = false; ++c->seq;
        return ATOM_CLIENT_HELLO;
    }
    if (boot != c->boot_id) {
        c->online = false; c->ack_id = 0; c->failures = 0; ++c->seq;
        return ATOM_CLIENT_RESTART;
    }
    uint32_t buttons = atom_read_le(p + 11, 3), event_buttons = atom_read_le(p + 23, 3);
    bool valid = (p[18] & 1) != 0;
    if (p[4] > 3 || p[5] > 3 || (p[6] & ~1u) || (p[7] & ~15u) ||
        (p[18] & ~3u) || (!valid && (p[18] || atom_read_le(p + 19, 4) || event_buttons || p[26])) ||
        (valid && !atom_read_le(p + 19, 4)) ||
        ((buttons | event_buttons) & (~ATOM_BUTTON_MASK | c->local_mask)) ||
        (p[8] > 10 && p[8] != 255) ||
        (p[4] != 3 && (p[8] != 255 || buttons || atom_read_le(p + 14, 4))))
        return atom_client_failure(c);
    c->failures = 0; c->mismatch = false; ++c->seq;
    if (payload) *payload = p;
    return ATOM_CLIENT_POLL;
}
