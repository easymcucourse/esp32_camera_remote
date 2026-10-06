#pragma once
#include "app_message.h"
bool display_bench_command(int argc,char **argv);
void display_bench_event(const app_message_t *message);
void display_bench_poll(void);
