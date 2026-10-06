#pragma once
#include "app_message.h"
#include <stdatomic.h>

struct app_message_lease {
    atomic_bool used;
    atomic_uint references;
    void *buffer, *context;
    size_t length;
    bool writable;
    app_message_lease_return_fn returned;
};
bool app_message_lease_retain(app_message_lease_t *lease);
unsigned app_message_lease_count(void);
void app_console_router_poll(void);
