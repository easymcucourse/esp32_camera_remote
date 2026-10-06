#include "app_core_mode.h"
#include <assert.h>
#include <pthread.h>
typedef struct { app_core_mode_t *mode;atomic_bool *go;bool normal,result; } contender_t;
static void *contend(void *value)
{
    contender_t *c=value;while(!atomic_load(c->go)) {}
    c->result=c->normal?app_core_mode_enter_normal(c->mode):app_core_mode_request_maintenance(c->mode);return NULL;
}
int main(void)
{
    assert(app_core_mode_get(NULL)==APP_CORE_RESTART);
    assert(!app_core_mode_enter_normal(NULL) && !app_core_mode_activate(NULL) && !app_core_mode_request_maintenance(NULL));
    app_core_mode_restart(NULL);
    app_core_mode_t boot=APP_CORE_MODE_INITIALIZER;
    assert(!app_core_mode_booting(&boot));app_core_mode_boot_begin(&boot);
    assert(app_core_mode_booting(&boot) && app_core_mode_request_maintenance(&boot));
    assert(!app_core_mode_activate(&boot));app_core_mode_boot_end(&boot);
    assert(!app_core_mode_booting(&boot) && app_core_mode_activate(&boot));
    app_core_mode_t normal=APP_CORE_MODE_INITIALIZER;
    assert(app_core_mode_get(&normal)==APP_CORE_STARTUP && !app_core_mode_activate(&normal));
    assert(app_core_mode_enter_normal(&normal) && app_core_mode_enter_normal(&normal));
    assert(!app_core_mode_request_maintenance(&normal) && !app_core_mode_activate(&normal));
    app_core_mode_restart(&normal);assert(!app_core_mode_enter_normal(&normal));
    for(unsigned i=0;i<300;++i) {
        app_core_mode_t mode=APP_CORE_MODE_INITIALIZER;atomic_bool go=false;
        contender_t a={&mode,&go,true,false},b={&mode,&go,false,false};pthread_t first,second;
        assert(!pthread_create(&first,NULL,contend,&a) && !pthread_create(&second,NULL,contend,&b));
        atomic_store(&go,true);assert(!pthread_join(first,NULL) && !pthread_join(second,NULL));
        assert(a.result!=b.result);
        if(a.result)assert(app_core_mode_get(&mode)==APP_CORE_NORMAL && !app_core_mode_request_maintenance(&mode));
        else {
            assert(app_core_mode_get(&mode)==APP_CORE_ACTIVATING && !app_core_mode_enter_normal(&mode));
            assert(!app_core_mode_request_maintenance(&mode));
            assert(app_core_mode_activate(&mode) && !app_core_mode_activate(&mode));
            assert(app_core_mode_get(&mode)==APP_CORE_MAINTENANCE && !app_core_mode_enter_normal(&mode));
        }
        app_core_mode_restart(&mode);
        assert(app_core_mode_get(&mode)==APP_CORE_RESTART && !app_core_mode_enter_normal(&mode) && !app_core_mode_request_maintenance(&mode));
    }
    return 0;
}
