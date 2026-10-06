#include <assert.h>
#include <stdio.h>
#include "wifi_apply.h"
static unsigned saves, restarts, fail_save, fail_restart;
static network_config_t stored, driver;
static bool save(void *ctx, const network_config_t *config)
{ (void)ctx; ++saves; if (saves == fail_save) return false; stored = *config; return true; }
static bool restart(void *ctx, const network_config_t *config)
{ (void)ctx; ++restarts; if (restarts == fail_restart) return false; driver = *config; return true; }
static void init(network_config_t *config)
{ network_config_make_default(config); stored = driver = *config; saves = restarts = fail_save = fail_restart = 0; }
int main(void)
{
    network_config_t current, next, old;
    init(&current); next = current;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_OK && !saves && !restarts);
    next.channel = 14;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_INVALID && !saves && !restarts);
    next = current; next.show_password = false;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_OK && saves == 1 && !restarts);
    assert(network_config_equal(&stored, &current) && !current.show_password);
    init(&current); old = current; next = current; next.channel = 11; fail_save = 1;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_SAVE_FAILED && !restarts);
    assert(network_config_equal(&current, &old) && network_config_equal(&stored, &old));
    init(&current); fail_restart = 1;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_DRIVER_FAILED);
    assert(saves == 2 && restarts == 2 && network_config_equal(&current, &old) && network_config_equal(&stored, &old) && network_config_equal(&driver, &old));
    init(&current); fail_restart = 1; fail_save = 2;
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_ROLLBACK_FAILED);
    assert(restarts == 2 && network_config_equal(&current, &old) && network_config_equal(&stored, &next));
    init(&current);
    assert(wifi_apply_config(&current, &next, 13, save, restart, NULL) == WIFI_APPLY_OK);
    assert(saves == 1 && restarts == 1 && network_config_equal(&current, &next) && network_config_equal(&driver, &next));
    puts("Wi-Fi transaction save, restart and rollback tests passed");
}
