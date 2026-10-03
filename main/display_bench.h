#pragma once
#include <stdbool.h>
bool display_bench_command(int argc, char **argv);
void display_bench_poll(void);
/* Console may be created before the initial camera start. */
void display_bench_ready(void);
