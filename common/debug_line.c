#include "debug_line.h"
bool debug_request_id(const char *token, uint32_t *id)
{
    if (!token || token[0] != '#' || !token[1] || !id) return false;
    uint32_t value = 0;
    for (const char *p = token + 1; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        unsigned digit = (unsigned)(*p - '0');
        if (value > (UINT32_MAX - digit) / 10) return false;
        value = value * 10 + digit;
    }
    *id = value; return true;
}
debug_line_result_t debug_line_feed(debug_line_t *line, uint8_t byte)
{
    if (byte == '\r' || byte == '\n') {
        bool invalid = line->discard;
        size_t length = line->length;
        line->text[length] = 0;
        line->length = 0; line->discard = false;
        return invalid ? DEBUG_LINE_ERROR : length ? DEBUG_LINE_READY : DEBUG_LINE_WAIT;
    }
    if (line->discard) return DEBUG_LINE_WAIT;
    if (byte == 8 || byte == 127) { if (line->length) --line->length; return DEBUG_LINE_WAIT; }
    if ((byte < 32 && byte != '\t') || byte > 126 || line->length == DEBUG_LINE_CAPACITY - 1)
        line->discard = true;
    else line->text[line->length++] = (char)byte;
    return DEBUG_LINE_WAIT;
}
