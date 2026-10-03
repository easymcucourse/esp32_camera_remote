#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "maint_auth.h"
static uint32_t random_number(void *ctx) { uint32_t *n=ctx;return (*n)++; }
int main(void)
{
    maint_auth_t a;uint32_t n=4293999999u;char token[33],old[33];
    maint_auth_reset(&a,random_number,&n);
    assert(!strcmp(a.pin,"999999"));
    n=4294967295u;maint_auth_reset(&a,random_number,&n);assert(!strcmp(a.pin,"000000"));
    for (unsigned i=0;i<4;++i) assert(maint_auth_login(&a,"wrong",UINT32_MAX-1000,random_number,&n,token)==MAINT_AUTH_BAD_PIN);
    assert(maint_auth_login(&a,"000001",UINT32_MAX-1000,random_number,&n,token)==MAINT_AUTH_LOCKED);
    assert(strcmp(a.pin,"000000") && !a.session);
    assert(maint_auth_retry_after(&a,UINT32_MAX-1000)==60);
    assert(maint_auth_login(&a,a.pin,50,random_number,&n,token)==MAINT_AUTH_LOCKED);
    assert(maint_auth_login(&a,a.pin,58999,random_number,&n,token)==MAINT_AUTH_OK);
    assert(maint_auth_check(&a,token));strcpy(old,token);
    assert(maint_auth_login(&a,a.pin,60000,random_number,&n,token)==MAINT_AUTH_OK);
    assert(!maint_auth_check(&a,old) && maint_auth_check(&a,token));
    token[0]='z';assert(!maint_auth_check(&a,token));
    assert(!maint_auth_check(&a,"") && !maint_auth_check(&a,NULL));
    maint_auth_logout(&a);assert(!maint_auth_check(&a,old));
    maint_auth_reset(&a,random_number,&n);assert(!a.session && !a.failures && !a.locked);
    return 0;
}
