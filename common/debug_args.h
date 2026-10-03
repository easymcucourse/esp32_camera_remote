#pragma once
#include <stddef.h>
/* In-place bounded tokenization: double quotes and backslash escapes.
 * Returns argc, -1 for malformed quoting, -2 for too many arguments. */
int debug_split_args(char *line, char **argv, size_t capacity);
