#include <assert.h>
#include <string.h>
#include "maint_json.h"
int main(void)
{
    const char *good[]={"{}"," \n{\"pin\":\"123456\"} ","{\"ssid\":\"{[abc]}\"}","{\"ssid\":\"a\\\"b\\\\c\",\"channel\":1}"};
    const char *bad[]={"", "[]", "{\"nested\":{}}", "{\"list\":[]}","{}{}","{", "}","{\"ssid\":\"a\\u0000b\"}","{\"ssid\":\"unterminated}"};
    for (unsigned i=0;i<sizeof(good)/sizeof(good[0]);++i) assert(maint_json_flat(good[i],strlen(good[i])));
    for (unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);++i) assert(!maint_json_flat(bad[i],strlen(bad[i])));
    assert(!maint_json_flat(NULL,1));assert(!maint_json_flat("{}\0suffix",10));
    char deep[512];memset(deep,'[',sizeof(deep));assert(!maint_json_flat(deep,sizeof(deep)));
    return 0;
}
