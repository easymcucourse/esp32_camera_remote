#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Concatenated or whitespace-separated bytes, exact expected length. */
bool debug_hex_bytes(int argc,char **argv,uint8_t *out,size_t expected);
