#include "debug_args.h"
int debug_split_args(char *line, char **argv, size_t capacity)
{
    if (!line || !argv) return -1;
    char *read = line, *write = line; size_t count = 0;
    while (*read) {
        while (*read == ' ' || *read == '\t') ++read;
        if (!*read) break;
        if (count == capacity) return -2;
        argv[count++] = write;
        char quoted = 0;
        while (*read && (quoted || (*read != ' ' && *read != '\t'))) {
            char c = *read++;
            if ((c == '"' || c == '\'') && (!quoted || c == quoted)) quoted = quoted ? 0 : c;
            else if (c == '\\') { if (!*read) return -1; *write++ = *read++; }
            else *write++ = c;
        }
        if (quoted) return -1;
        while (*read == ' ' || *read == '\t') ++read;
        *write++ = 0;
    }
    return (int)count;
}
