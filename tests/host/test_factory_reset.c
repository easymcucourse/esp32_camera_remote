#include <assert.h>
#include <stdio.h>
#include "app_core_factory.h"
typedef struct {
    bool acquire_ok, fail_save, fail_forget, fail_rollback, held, forgot, fail_ui, ui_reset;
    unsigned saves, releases, sequence;
    network_config_t store;
} fake_t;
static bool acquire(void *context)
{
    fake_t *f = context; assert(f->sequence++ == 0);
    f->held = f->acquire_ok; return f->held;
}
static void release(void *context)
{ fake_t *f = context; assert(f->held); f->held = false; ++f->releases; }
static bool save(void *context, const network_config_t *config)
{
    fake_t *f = context; assert(f->held); ++f->saves;
    if (f->saves == 1) {
        assert(f->sequence++ == 1 && config->channel == 6);
        /* Model an erase that mutates persistence before its commit fails. */
        f->store = *config; return !f->fail_save;
    }
    assert(f->saves == 2); if (f->fail_rollback) return false;
    f->store = *config; return true;
}
static bool forget(void *context)
{
    fake_t *f = context; assert(f->held && f->saves == 1 && f->sequence++ == 2);
    f->forgot = true; return !f->fail_forget;
}
static bool reset_ui(void *context)
{ fake_t *f=context;assert(f->held && f->forgot);f->ui_reset=true;return !f->fail_ui; }
int main(void)
{
    const factory_reset_ops_t ops = {acquire, release, save, forget,reset_ui};
    network_config_t current; network_config_make_default(&current); current.channel = 11;
    fake_t f = {.store = current};
    assert(factory_reset_all(&current, &ops, &f) == FACTORY_RESET_BUSY);
    assert(!f.saves && !f.releases && !f.forgot && f.store.channel == 11);
    f = (fake_t){.store = current, .acquire_ok = true, .fail_save = true};
    assert(factory_reset_all(&current, &ops, &f) == FACTORY_RESET_SAVE_FAILED);
    assert(f.saves == 2 && f.releases == 1 && !f.forgot && f.store.channel == 11 && !f.held);
    f = (fake_t){.store = current, .acquire_ok = true, .fail_forget = true};
    assert(factory_reset_all(&current, &ops, &f) == FACTORY_RESET_IDENTITY_FAILED);
    assert(f.saves == 2 && f.releases == 1 && f.forgot && f.store.channel == 11 && !f.held);
    f = (fake_t){.store = current, .acquire_ok = true, .fail_save = true, .fail_rollback = true};
    assert(factory_reset_all(&current, &ops, &f) == FACTORY_RESET_ROLLBACK_FAILED);
    assert(f.releases == 1 && !f.held && !f.forgot);
    f = (fake_t){.store = current, .acquire_ok = true};
    assert(factory_reset_all(&current, &ops, &f) == FACTORY_RESET_OK);
    assert(f.saves == 1 && f.forgot && !f.releases && f.held && f.store.channel == 6);
    assert(f.ui_reset);
    f=(fake_t){.store=current,.acquire_ok=true,.fail_ui=true};
    assert(factory_reset_all(&current,&ops,&f)==FACTORY_RESET_UI_FAILED);
    assert(f.ui_reset && f.forgot && !f.held && f.store.channel==11);
    assert(factory_reset_all(NULL, &ops, &f) == FACTORY_RESET_INVALID);
    assert(factory_reset_all(&current, NULL, &f) == FACTORY_RESET_INVALID);
    puts("Factory camera lease, NVS failure, rollback and reboot gate tests passed");
    return 0;
}
