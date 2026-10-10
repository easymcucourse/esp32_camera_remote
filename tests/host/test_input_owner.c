#include "input_owner.h"
#include <assert.h>
#include <stdio.h>
static int64_t clock_us;
int64_t esp_timer_get_time(void) { return clock_us; }
static unsigned presses,releases;
static bool blocked;
static input_provider_handle_t concurrent_provider;
static input_report_t concurrent_report;
static bool emit(void *ctx,pad_action_t a)
{
    (void)ctx;
    if (a.type==PAD_ACTION_RELEASE_ALL) {
        ++releases;
        if (concurrent_provider) {
            input_provider_handle_t provider=concurrent_provider;concurrent_provider=0;
            clock_us+=10000;
            assert(input_provider_publish(provider,&concurrent_report)==ESP_OK);
        }
        return !blocked;
    }
    if (a.type==PAD_ACTION_MF_CANCEL) return true;
    if ((a.type==PAD_ACTION_S1 || a.type==PAD_ACTION_S2) && a.value) ++presses;
    return true;
}
int main(void)
{
    input_owner_t owner;
    gamepad_caps_t caps={.session=true,.generation=3};
    input_provider_handle_t atom,sim,new_atom;
    assert(input_provider_registry_init()==ESP_OK);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&atom)==ESP_OK);
    assert(input_provider_register(INPUT_SOURCE_UART_SIM,&sim)==ESP_OK);
    input_owner_init(&owner,emit,NULL);
    input_report_t r={.connected=true,.atom_online=true,.gimbal_fault=true,.source_epoch=1,.report_id=1,.battery=7};
    assert(input_provider_publish(atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,0);
    assert(owner.latest.connected && owner.latest.battery==7 && owner.latest.gimbal_fault && presses==0);
    r.report_id=2;r.rt=255;assert(input_provider_publish(atom,&r)==ESP_OK);
    input_owner_tick(&owner,&caps,50);assert(presses==2);
    unsigned old=releases;
    blocked=true;assert(!input_owner_select(&owner,INPUT_SOURCE_UART_SIM));
    r.report_id=1;r.sim=true;assert(input_provider_publish(sim,&r)==ESP_OK);
    input_owner_tick(&owner,&caps,100);assert(presses==2 && !owner.latest.connected && releases>old);
    blocked=false;r.report_id=2;assert(input_provider_publish(sim,&r)==ESP_OK);
    input_owner_tick(&owner,&caps,150);assert(presses==2 && owner.latest.sim);
    r.report_id=3;r.rt=0;assert(input_provider_publish(sim,&r)==ESP_OK);input_owner_tick(&owner,&caps,200);
    r.report_id=4;r.rt=255;assert(input_provider_publish(sim,&r)==ESP_OK);input_owner_tick(&owner,&caps,250);
    assert(presses==4);
    r.report_id=10;assert(input_provider_publish(atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,300);
    assert(owner.latest.report_id==4 && owner.latest.sim && presses==4);
    assert(input_provider_disconnect(sim,INPUT_DISCONNECT_OFFLINE)==ESP_OK);input_owner_tick(&owner,&caps,350);
    assert(!owner.latest.connected && owner.latest.battery==255 && !owner.latest.gimbal_fault);

    assert(input_owner_select(&owner,INPUT_SOURCE_ATOM));
    assert(input_provider_unregister(atom)==ESP_OK);input_owner_tick(&owner,&caps,400);
    assert(input_provider_register(INPUT_SOURCE_ATOM,&new_atom)==ESP_OK && new_atom!=atom);
    r=(input_report_t){.connected=true,.atom_online=true,.source_epoch=1,.report_id=1,.rt=255,.battery=8};
    assert(input_provider_publish(new_atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,450);
    assert(owner.latest.battery==8 && presses==4); /* New handle permits epoch reset. */
    r.report_id=2;r.rt=0;assert(input_provider_publish(new_atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,500);
    r.report_id=3;r.rt=255;assert(input_provider_publish(new_atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,550);
    assert(presses==6);
    for (unsigned i=0;i<INPUT_PROVIDER_REPORT_CAPACITY;++i) {
        r.report_id=4+i;assert(input_provider_publish(new_atom,&r)==ESP_OK);
    }
    assert(input_provider_publish(new_atom,&r)==ESP_ERR_NO_MEM);
    input_owner_tick(&owner,&caps,600);
    assert(!owner.latest.connected && !owner.reports.gamepad.connected && presses==6);
    /* A fresh queue insertion timestamp is independent of protocol IDs.
     * A backlog older than one second must release and discard held RT. */
    clock_us=600000;r.report_id=100;r.rt=0;
    assert(input_provider_publish(new_atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,600);
    clock_us=650000;r.report_id=101;r.rt=255;
    assert(input_provider_publish(new_atom,&r)==ESP_OK);input_owner_tick(&owner,&caps,1651);
    assert(!owner.latest.connected && !owner.reports.gamepad.connected && presses==6);
    /* A report can arrive while a safety callback waits for another owner.
     * Its capture time is later than this tick's sampled now, not stale. */
    clock_us=1700000;r.source_epoch=2;r.report_id=200;r.rt=0;
    assert(input_provider_publish(new_atom,&r)==ESP_OK);
    concurrent_provider=new_atom;concurrent_report=r;concurrent_report.report_id=201;
    input_owner_tick(&owner,&caps,1700);
    assert(owner.latest.connected && owner.latest.report_id==201 && presses==6);
    /* The same ordering and genuine stale rejection survive ms wraparound. */
    clock_us=(int64_t)5*1000;r.report_id=202;
    assert(input_provider_publish(new_atom,&r)==ESP_OK);
    input_owner_tick(&owner,&caps,UINT32_MAX-5);
    assert(owner.latest.connected && owner.latest.report_id==202);
    clock_us=(int64_t)(UINT32_MAX-1000)*1000;r.report_id=203;
    assert(input_provider_publish(new_atom,&r)==ESP_OK);
    input_owner_tick(&owner,&caps,100);
    assert(!owner.latest.connected && !owner.reports.gamepad.connected && presses==6);
    blocked=true;assert(!input_owner_quiesce(&owner));
    assert(input_provider_publish(new_atom,&r)==ESP_ERR_INVALID_STATE);
    blocked=false;assert(input_owner_quiesce(&owner));assert(!owner.latest.connected);
    assert(!input_owner_select(&owner,INPUT_SOURCE_COUNT));
    puts("provider registry and report owner integration passed");
}
