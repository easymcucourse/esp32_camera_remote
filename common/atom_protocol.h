#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ATOM_PROTOCOL_VERSION = 2, ATOM_PROTOCOL_ADDRESS = 0x42,
    ATOM_REQUEST_SIZE = 9, ATOM_HELLO_SIZE = 12, ATOM_POLL_SIZE = 28,
    ATOM_RESPONSE_MAX = 35, ATOM_CMD_HELLO = 0x01, ATOM_CMD_POLL = 0x10,
    ATOM_LOCAL_MASK = 1u << 1, ATOM_BUTTON_MASK = (1u << 18) - 1,
    ATOM_DEBUG_SIM = 1u << 0,
    ATOM_INPUT_DS = 0, ATOM_INPUT_XBOX = 1, ATOM_FEATURE_INPUT_MODE = 1u << 4,
};
typedef enum {
    ATOM_OK = 0, ATOM_BAD_CRC, ATOM_BAD_VERSION, ATOM_UNKNOWN_CMD,
    ATOM_BAD_PARAM, ATOM_NOT_READY
} atom_status_t;
typedef struct { uint8_t seq, cmd; uint32_t param; } atom_request_t;
typedef struct {
    uint8_t bytes[ATOM_REQUEST_SIZE];
    size_t length;
    uint32_t last_ms;
} atom_receiver_t;
typedef struct {
    atom_status_t status;
    uint8_t version, length;
    const uint8_t *payload;
} atom_response_t;

uint8_t atom_crc8(const uint8_t *bytes, size_t length);
uint32_t atom_read_le(const uint8_t *bytes, unsigned width);
void atom_write_le(uint8_t *bytes, uint32_t value, unsigned width);
void atom_encode_request(uint8_t out[ATOM_REQUEST_SIZE], atom_request_t request);
/* Feed one byte. A completed candidate returns true, including CRC/version
 * errors that need a response; malformed candidates retain a possible suffix. */
bool atom_receiver_feed(atom_receiver_t *receiver, uint8_t byte, uint32_t now_ms,
                        atom_request_t *request, atom_status_t *status);
size_t atom_encode_response(uint8_t out[ATOM_RESPONSE_MAX], atom_request_t request,
                            atom_status_t status, const uint8_t *payload, uint8_t length);
/* Validates complete header, sequence, command, length and CRC. Error replies
 * have length zero; trailing bytes from a longer master read are ignored. */
bool atom_decode_response(const uint8_t *bytes, size_t size, atom_request_t request,
                          uint8_t success_length, atom_response_t *response);
