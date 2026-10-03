#include "atom_client.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned completed, good;
static void feed(atom_receiver_t *rx, uint8_t byte, uint32_t now)
{
    atom_request_t request; atom_status_t status;
    if (atom_receiver_feed(rx, byte, now, &request, &status)) {
        ++completed;
        if (status == ATOM_OK) { ++good; assert(request.cmd == ATOM_CMD_POLL && request.param == 0xffffffff); }
    }
}
static atom_client_result_t reply(atom_client_t *c, uint8_t *p, uint8_t length, atom_status_t status)
{
    uint8_t bytes[ATOM_RESPONSE_MAX];
    size_t size = atom_encode_response(bytes, atom_client_request(c), status, p, length);
    const uint8_t *out;
    return atom_client_response(c, bytes, size, &out);
}
int main(void)
{
    assert(atom_crc8((const uint8_t *)"123456789", 9) == 0xf4);
    uint8_t command[ATOM_REQUEST_SIZE];
    atom_request_t request = {255, ATOM_CMD_POLL, 0xffffffff};
    atom_encode_request(command, request);
    atom_receiver_t rx = {0};
    for (unsigned i = 0; i < sizeof(command); ++i) feed(&rx, command[i], i);
    assert(completed == 1 && good == 1 && !rx.length);
    /* Drop or insert one byte at every position, then recover on the next frame. */
    for (unsigned pos = 0; pos < sizeof(command); ++pos) {
        rx = (atom_receiver_t){0}; good = completed = 0;
        for (unsigned i = 0; i < sizeof(command); ++i) if (i != pos) feed(&rx, command[i], 1);
        for (unsigned i = 0; i < sizeof(command); ++i) feed(&rx, command[i], 2);
        assert(good == 1);
        rx = (atom_receiver_t){0}; good = completed = 0;
        for (unsigned i = 0; i < sizeof(command); ++i) {
            if (i == pos) feed(&rx, 0x55, 1);
            feed(&rx, command[i], 1);
        }
        for (unsigned i = 0; i < sizeof(command); ++i) feed(&rx, command[i], 2);
        assert(good >= 1 && rx.length == 0);
    }
    rx = (atom_receiver_t){0}; good = completed = 0;
    for (unsigned i = 0; i < 4; ++i) feed(&rx, command[i], UINT32_MAX - 10);
    for (unsigned i = 0; i < sizeof(command); ++i) feed(&rx, command[i], 10);
    assert(good == 1); /* 21 ms timeout across clock wrap. */
    for (unsigned i = 0; i < 100; ++i) feed(&rx, 0, 11);
    assert(completed == 1);
    command[1] = 1; command[8] = atom_crc8(command, 8);
    atom_request_t parsed; atom_status_t status = ATOM_OK;
    for (unsigned i = 0; i < sizeof(command); ++i)
        if (atom_receiver_feed(&rx, command[i], 12, &parsed, &status)) assert(status == ATOM_BAD_VERSION);

    uint8_t p[ATOM_POLL_SIZE] = {0}, bytes[ATOM_RESPONSE_MAX]; atom_response_t response;
    for (unsigned i = 0; i < sizeof(p); ++i) p[i] = (uint8_t)i;
    size_t size = atom_encode_response(bytes, request, ATOM_OK, p, sizeof(p));
    assert(size == sizeof(bytes) && atom_decode_response(bytes, size, request, sizeof(p), &response));
    for (unsigned i = 0; i < size; ++i) {
        bytes[i] ^= 1;
        assert(!atom_decode_response(bytes, size, request, sizeof(p), &response)); bytes[i] ^= 1;
    }
    for (unsigned i = 0; i < size; ++i) assert(!atom_decode_response(bytes, i, request, sizeof(p), &response));
    memset(bytes, 0x99, sizeof(bytes));
    assert(atom_encode_response(bytes, request, ATOM_BAD_CRC, NULL, 0) == 7);
    assert(atom_decode_response(bytes, sizeof(bytes), request, sizeof(p), &response) && response.length == 0);
    assert(response.status == ATOM_BAD_CRC);

    atom_client_t c = {0}; uint8_t hello[ATOM_HELLO_SIZE] = {0};
    atom_write_le(hello, 123, 4); hello[6] = 9; hello[7] = 128; hello[8] = 2;
    assert(reply(&c, hello, sizeof(hello), ATOM_OK) == ATOM_CLIENT_HELLO);
    assert(c.online && c.boot_id == 123 && c.seq == 1 && c.local_mask == 2 && !c.ack_id);
    memset(p, 0, sizeof(p)); atom_write_le(p, 123, 4); p[4] = 3; p[8] = 5;
    p[18] = 3; atom_write_le(p + 19, 0xffffffff, 4); atom_write_le(p + 23, 8, 3);
    assert(reply(&c, p, sizeof(p), ATOM_OK) == ATOM_CLIENT_POLL);
    p[27]=ATOM_DEBUG_SIM;
    assert(reply(&c,p,sizeof(p),ATOM_OK)==ATOM_CLIENT_POLL);
    p[27]=2;
    assert(reply(&c,p,sizeof(p),ATOM_OK)==ATOM_CLIENT_POLL); /* New neutral source, same SIM flag. */
    p[27]=0;
    assert(reply(&c,p,sizeof(p),ATOM_OK)==ATOM_CLIENT_POLL);
    c.ack_id = 0xffffffff; atom_request_t before = atom_client_request(&c);
    assert(atom_client_failure(&c) == ATOM_CLIENT_RETRY);
    assert(atom_client_failure(&c) == ATOM_CLIENT_RETRY);
    atom_request_t after = atom_client_request(&c);
    assert(before.seq == after.seq && before.param == after.param);
    assert(reply(&c, p, sizeof(p), ATOM_OK) == ATOM_CLIENT_POLL && !c.failures);
    atom_write_le(p, 456, 4);
    assert(reply(&c, p, sizeof(p), ATOM_OK) == ATOM_CLIENT_RESTART && !c.online && !c.ack_id);
    assert(atom_client_request(&c).cmd == ATOM_CMD_HELLO);
    assert(reply(&c, hello, sizeof(hello), ATOM_BAD_VERSION) == ATOM_CLIENT_MISMATCH);
    assert(c.mismatch);
    assert(reply(&c, hello, sizeof(hello), ATOM_OK) == ATOM_CLIENT_HELLO);
    atom_write_le(p, 123, 4); p[23] = 2; /* Local L3 bit must never be forwarded. */
    assert(reply(&c, p, sizeof(p), ATOM_OK) == ATOM_CLIENT_RETRY);
    assert(atom_client_failure(&c) == ATOM_CLIENT_RETRY);
    assert(atom_client_failure(&c) == ATOM_CLIENT_OFFLINE && !c.online && !c.ack_id);
    puts("ATOM v2 framing, resynchronization and client recovery tests passed");
}
