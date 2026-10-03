#include "debug_line.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    debug_line_t line = {0};
    uint32_t id = 7;
    assert(debug_request_id("#4294967295", &id) && id == UINT32_MAX);
    assert(debug_request_id("#0", &id) && id == 0);
    assert(!debug_request_id("#4294967296", &id) && id == 0);
    assert(!debug_request_id("#1x", &id) && !debug_request_id("#-1", &id));
    assert(!debug_request_id("#", &id) && !debug_request_id("1", &id));
    const char *text = "verx\bsi\177ion\r\n";
    unsigned lines = 0;
    for (const char *p = text; *p; ++p) {
        debug_line_result_t result = debug_line_feed(&line, (uint8_t)*p);
        if (result == DEBUG_LINE_READY) { ++lines; assert(!strcmp(line.text, "version")); }
        else assert(result == DEBUG_LINE_WAIT);
    }
    assert(lines == 1);
    for (unsigned i = 0; i < 255; ++i) assert(debug_line_feed(&line, 'x') == DEBUG_LINE_WAIT);
    assert(debug_line_feed(&line, '\n') == DEBUG_LINE_READY && strlen(line.text) == 255);
    for (unsigned i = 0; i < 256; ++i) debug_line_feed(&line, 'x');
    debug_line_feed(&line, 8); /* Backspace cannot turn a truncated line into a valid command. */
    assert(debug_line_feed(&line, '\r') == DEBUG_LINE_ERROR);
    assert(debug_line_feed(&line, '\n') == DEBUG_LINE_WAIT);
    debug_line_feed(&line, 's'); debug_line_feed(&line, 0); debug_line_feed(&line, 'j');
    assert(debug_line_feed(&line, '\n') == DEBUG_LINE_ERROR);
    debug_line_feed(&line, 's'); assert(debug_line_feed(&line, '\n') == DEBUG_LINE_READY);
    assert(!strcmp(line.text, "s"));
    debug_line_feed(&line, 255); assert(debug_line_feed(&line, '\n') == DEBUG_LINE_ERROR);
    puts("UART CRLF, editing, maximum length, discard and next-line recovery passed");
}
