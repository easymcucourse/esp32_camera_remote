#include "input_provider_registry.h"
#include <assert.h>
#include <stdio.h>
int64_t esp_timer_get_time(void) { return 0; }

int main(void)
{
    input_provider_handle_t atom=99,sim=0,other=0;
    input_report_t r={.connected=true,.atom_online=true,.battery=10,.source_epoch=1,.report_id=1};
    input_provider_event_t e;
    assert(input_provider_register(INPUT_SOURCE_ATOM,&atom)==ESP_ERR_INVALID_STATE && atom==0);
    assert(input_provider_registry_init()==ESP_OK);
    assert(input_provider_registry_init()==ESP_ERR_INVALID_STATE);
    assert(input_provider_register(INPUT_SOURCE_COUNT,&atom)==ESP_ERR_INVALID_ARG);
    assert(input_provider_register(INPUT_SOURCE_ATOM,NULL)==ESP_ERR_INVALID_ARG);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&atom)==ESP_OK && atom);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&other)==ESP_ERR_INVALID_STATE && !other);
    assert(input_provider_register(INPUT_SOURCE_UART_SIM,&sim)==ESP_OK && sim!=atom);
    r.atom_online=false;assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_ARG);
    r.sim=true;assert(input_provider_publish(atom,&r)==ESP_OK);
    assert(input_provider_registry_next(&e) && e.report.sim && !e.report.atom_online);
    r.sim=false;r.atom_online=true;r.mismatch=true;
    assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_ARG);r.mismatch=false;
    assert(input_provider_publish(atom,&r)==ESP_OK);
    r.buttons=1; /* Queue owns the value, not the caller's storage. */
    assert(input_provider_registry_next(&e));
    assert(e.handle==atom && e.source==INPUT_SOURCE_ATOM && !e.disconnected && !e.report.buttons);
    assert(input_provider_registry_idle());
    r.rx=128;assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_ARG);r.rx=-128;
    r.battery=11;assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_ARG);r.battery=255;
    r.source_epoch=0;assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_ARG);r.source_epoch=1;
    assert(input_provider_publish(0,&r)==ESP_ERR_INVALID_STATE);

    /* Overflow discards old reports only for that source. Safety notifications
     * have reserved storage independent of a completely full report ring. */
    assert(input_provider_publish(sim,&r)==ESP_OK);
    for (unsigned i=1;i<INPUT_PROVIDER_REPORT_CAPACITY;++i) {
        r.report_id=i; assert(input_provider_publish(atom,&r)==ESP_OK);
    }
    assert(input_provider_publish(atom,&r)==ESP_ERR_NO_MEM);
    assert(input_provider_registry_next(&e) && e.disconnected && e.handle==atom &&
        e.reason==INPUT_DISCONNECT_OVERFLOW);
    assert(input_provider_registry_next(&e) && !e.disconnected && e.handle==sim);
    assert(!input_provider_registry_next(&e) && input_provider_registry_idle());
    r.report_id=20;assert(input_provider_publish(atom,&r)==ESP_OK);
    assert(input_provider_registry_next(&e) && !e.disconnected && e.report.report_id==20);

    assert(input_provider_publish(atom,&r)==ESP_OK);
    assert(input_provider_disconnect(atom,INPUT_DISCONNECT_RESTART)==ESP_OK);
    assert(input_provider_registry_next(&e) && e.disconnected && e.reason==INPUT_DISCONNECT_RESTART);
    assert(!input_provider_registry_next(&e));
    assert(input_provider_disconnect(atom,INPUT_DISCONNECT_REASON_COUNT)==ESP_ERR_INVALID_ARG);
    assert(input_provider_unregister(atom)==ESP_OK);
    assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_STATE);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&other)==ESP_ERR_INVALID_STATE);
    assert(input_provider_registry_next(&e) && e.disconnected && e.handle==atom);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&other)==ESP_OK && other!=atom);
    assert(input_provider_disconnect(atom,INPUT_DISCONNECT_OFFLINE)==ESP_ERR_INVALID_STATE);
    assert(input_provider_publish(atom,&r)==ESP_ERR_INVALID_STATE);
    assert(input_provider_publish(other,&r)==ESP_OK);
    assert(input_provider_publish(sim,&r)==ESP_OK);
    input_provider_registry_close();
    assert(input_provider_publish(other,&r)==ESP_ERR_INVALID_STATE);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&atom)==ESP_ERR_INVALID_STATE);
    assert(input_provider_registry_next(&e) && e.disconnected && e.handle==other);
    assert(input_provider_registry_next(&e) && e.disconnected && e.handle==sim);
    assert(!input_provider_registry_next(&e) && input_provider_registry_idle());
    assert(!input_provider_registry_next(NULL));
    assert(input_provider_registry_deinit()==ESP_ERR_INVALID_STATE);
    assert(input_provider_unregister(other)==ESP_OK);
    assert(input_provider_unregister(sim)==ESP_OK);
    assert(input_provider_registry_deinit()==ESP_OK);
    assert(input_provider_registry_init()==ESP_OK);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&atom)==ESP_OK && atom>other && atom>sim);
    assert(input_provider_publish(other,&r)==ESP_ERR_INVALID_STATE);
    puts("provider copy, stale handles, overflow safety, restart and stop passed");
}
