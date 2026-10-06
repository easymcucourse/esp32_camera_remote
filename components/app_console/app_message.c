#include "app_message_internal.h"
#include <limits.h>

static app_message_lease_t leases[APP_MESSAGE_LEASE_CAPACITY];
static atomic_uint live;

esp_err_t app_message_lease_create(void *buffer, size_t length, bool writable,
    app_message_lease_return_fn returned, void *context, app_message_lease_t **out)
{
    if (!buffer || !length || !returned || !out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    for (unsigned i = 0; i < APP_MESSAGE_LEASE_CAPACITY; ++i) {
        bool expected = false;
        if (!atomic_compare_exchange_strong(&leases[i].used, &expected, true)) continue;
        app_message_lease_t *lease = &leases[i];
        lease->buffer = buffer; lease->length = length; lease->writable = writable;
        lease->returned = returned; lease->context = context;
        atomic_store(&lease->references, 1);
        atomic_fetch_add(&live, 1); *out = lease;
        return ESP_OK;
    }
    return ESP_ERR_NO_MEM;
}

bool app_message_lease_retain(app_message_lease_t *lease)
{
    if (!lease || !atomic_load(&lease->used)) return false;
    unsigned count = atomic_load(&lease->references);
    while (count) {
        if (count == UINT_MAX) return false;
        if (atomic_compare_exchange_weak(&lease->references, &count, count + 1)) return true;
    }
    return false;
}

const void *app_message_lease_data(const app_message_lease_t *lease, size_t *length)
{
    if (!lease || !atomic_load(&lease->used) || !atomic_load(&lease->references)) return NULL;
    if (length) *length = lease->length;
    return lease->buffer;
}

void *app_message_lease_write(app_message_lease_t *lease, size_t *length)
{
    if (!lease || !lease->writable) return NULL;
    return (void *)app_message_lease_data(lease, length);
}

void app_message_release(app_message_t *message)
{
    if (!message || !message->lease) return;
    app_message_lease_t *lease = message->lease;
    message->lease = NULL;
    if (atomic_fetch_sub(&lease->references, 1) != 1) return;
    lease->returned(lease->context);
    lease->buffer = NULL; lease->returned = NULL;
    atomic_fetch_sub(&live, 1);
    atomic_store(&lease->used, false);
}

unsigned app_message_lease_count(void) { return atomic_load(&live); }
