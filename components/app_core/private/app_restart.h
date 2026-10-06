#pragma once
#include <stdbool.h>
bool app_restart_prepare(void);
bool app_restart_commit(unsigned delay_ms);
void app_restart_cancel(void);
bool app_restart_due(void);
bool app_restart_pending(void);
