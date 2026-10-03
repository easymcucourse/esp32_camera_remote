#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define DEBUG_LINE_CAPACITY 256
typedef struct { char text[DEBUG_LINE_CAPACITY]; size_t length; bool discard; } debug_line_t;
typedef enum { DEBUG_LINE_WAIT, DEBUG_LINE_READY, DEBUG_LINE_ERROR } debug_line_result_t;
/* READY text remains valid until the next feed; CRLF emits only one result.
 * Invalid/overlong lines are discarded through the next terminator. */
debug_line_result_t debug_line_feed(debug_line_t *line, uint8_t byte);
bool debug_request_id(const char *token, uint32_t *id);
