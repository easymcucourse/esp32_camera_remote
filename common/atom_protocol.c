#include "atom_protocol.h"
#include <string.h>

uint8_t atom_crc8(const uint8_t *bytes, size_t length)
{
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (uint8_t)((crc << 1) ^ ((crc & 0x80) ? 0x07 : 0));
    }
    return crc;
}
uint32_t atom_read_le(const uint8_t *bytes, unsigned width)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < width && i < 4; ++i) value |= (uint32_t)bytes[i] << (8 * i);
    return value;
}
void atom_write_le(uint8_t *bytes, uint32_t value, unsigned width)
{
    for (unsigned i = 0; i < width && i < 4; ++i) bytes[i] = (uint8_t)(value >> (8 * i));
}
void atom_encode_request(uint8_t out[ATOM_REQUEST_SIZE], atom_request_t request)
{
    out[0] = 0xa5; out[1] = ATOM_PROTOCOL_VERSION;
    out[2] = request.seq; out[3] = request.cmd;
    atom_write_le(out + 4, request.param, 4);
    out[8] = atom_crc8(out, 8);
}
bool atom_receiver_feed(atom_receiver_t *r, uint8_t byte, uint32_t now,
                        atom_request_t *request, atom_status_t *status)
{
    if (!r || !request || !status) return false;
    if (r->length && (uint32_t)(now - r->last_ms) >= 20) r->length = 0;
    r->last_ms = now;
    if (!r->length && byte != 0xa5) return false;
    r->bytes[r->length++] = byte;
    if (r->length != ATOM_REQUEST_SIZE) return false;
    *request = (atom_request_t){r->bytes[2], r->bytes[3], atom_read_le(r->bytes + 4, 4)};
    *status = atom_crc8(r->bytes, 8) != r->bytes[8] ? ATOM_BAD_CRC :
        r->bytes[1] != ATOM_PROTOCOL_VERSION ? ATOM_BAD_VERSION : ATOM_OK;
    r->length = 0;
    if (*status == ATOM_BAD_CRC) {
        for (size_t i = 1; i < ATOM_REQUEST_SIZE; ++i) {
            if (r->bytes[i] == 0xa5) {
                r->length = ATOM_REQUEST_SIZE - i;
                memmove(r->bytes, r->bytes + i, r->length);
                break;
            }
        }
    }
    return true;
}
size_t atom_encode_response(uint8_t out[ATOM_RESPONSE_MAX], atom_request_t request,
                            atom_status_t status, const uint8_t *payload, uint8_t length)
{
    if (!out || status > ATOM_NOT_READY || length > ATOM_POLL_SIZE || (length && !payload)) return 0;
    if (status != ATOM_OK) length = 0;
    out[0] = 0x5a; out[1] = ATOM_PROTOCOL_VERSION;
    out[2] = request.seq; out[3] = request.cmd; out[4] = (uint8_t)status; out[5] = length;
    if (length) memcpy(out + 6, payload, length);
    out[6 + length] = atom_crc8(out, 6 + length);
    return 7 + length;
}
bool atom_decode_response(const uint8_t *bytes, size_t size, atom_request_t request,
                          uint8_t success_length, atom_response_t *response)
{
    if (!bytes || !response || size < 7 || bytes[0] != 0x5a ||
        bytes[2] != request.seq || bytes[3] != request.cmd || bytes[4] > ATOM_NOT_READY) return false;
    uint8_t length = bytes[5];
    if (length > ATOM_POLL_SIZE || size < (size_t)length + 7 ||
        (bytes[4] == ATOM_OK ? length != success_length : length != 0) ||
        atom_crc8(bytes, 6 + length) != bytes[6 + length]) return false;
    *response = (atom_response_t){(atom_status_t)bytes[4], bytes[1], length, bytes + 6};
    return true;
}
