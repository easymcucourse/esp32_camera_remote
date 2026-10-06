#include "maint_auth.h"
#include <string.h>

static bool equal(const uint8_t *a, const uint8_t *b, unsigned size)
{
    unsigned difference = 0;
    for (unsigned i=0;i<size;++i) difference |= a[i]^b[i];
    return difference == 0;
}
static void new_pin(char pin[7], maint_random_fn random, void *context)
{
    uint32_t value;
    do { value=random(context); } while (value>=4294000000u);
    value%=1000000u;
    pin[6]=0;
    for (unsigned i=6;i>0;--i) { pin[i-1]='0'+value%10; value/=10; }
}
void maint_auth_logout(maint_auth_t *a) { memset(a->token,0,sizeof(a->token));a->session=false; }
void maint_auth_reset(maint_auth_t *a, maint_random_fn random, void *context)
{
    memset(a,0,sizeof(*a)); new_pin(a->pin,random,context);
}
unsigned maint_auth_retry_after(const maint_auth_t *a,uint32_t now)
{
    if (!a->locked || (int32_t)(now-a->locked_until_ms)>=0) return 0;
    return (a->locked_until_ms-now+999)/1000;
}
maint_auth_result_t maint_auth_login(maint_auth_t *a,const char *pin,uint32_t now,
                                     maint_random_fn random,void *context,char token[33])
{
    token[0]=0;
    if (maint_auth_retry_after(a,now)) return MAINT_AUTH_LOCKED;
    a->locked=false;
    bool valid=pin && strlen(pin)==6;
    if (valid) valid=equal((const uint8_t*)pin,(const uint8_t*)a->pin,6);
    if (!valid) {
        if (++a->failures<5) return MAINT_AUTH_BAD_PIN;
        a->failures=0; a->locked=true; a->locked_until_ms=now+60000;
        char previous[7];memcpy(previous,a->pin,sizeof(previous));
        do { new_pin(a->pin,random,context); } while (!memcmp(previous,a->pin,6));
        return MAINT_AUTH_LOCKED;
    }
    a->failures=0;
    unsigned nonzero;
    do {
        nonzero=0;
        for (unsigned i=0;i<16;i+=4) {
            uint32_t value=random(context);
            for (unsigned j=0;j<4;++j) { a->token[i+j]=(uint8_t)(value>>(8*j));nonzero|=a->token[i+j]; }
        }
    } while (!nonzero);
    static const char hex[]="0123456789abcdef";
    for (unsigned i=0;i<16;++i) { token[2*i]=hex[a->token[i]>>4];token[2*i+1]=hex[a->token[i]&15]; }
    token[32]=0; a->session=true; return MAINT_AUTH_OK;
}
bool maint_auth_check(const maint_auth_t *a,const char *token)
{
    if (!a->session || !token || strlen(token)!=32) return false;
    uint8_t decoded[16];unsigned invalid=0;
    for (unsigned i=0;i<32;++i) {
        unsigned char c=(unsigned char)token[i];
        unsigned n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;
        invalid|=n>>4;
        if (!(i&1)) decoded[i/2]=(uint8_t)(n<<4); else decoded[i/2]|=(uint8_t)n;
    }
    return !invalid && equal(decoded,a->token,16);
}
