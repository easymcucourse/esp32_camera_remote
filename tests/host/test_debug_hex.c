#include "debug_hex.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    uint8_t out[4]={0};
    char *joined[]={"a502FF00"};
    const uint8_t expected[]={0xa5,2,255,0};
    assert(debug_hex_bytes(1,joined,out,4) && !memcmp(out,expected,4));
    char *split[]={"a5","02", "FF 00"};
    assert(debug_hex_bytes(3,split,out,4) && !memcmp(out,expected,4));
    char *odd[]={"a","5"}, *invalid[]={"a5xx"}, *short_frame[]={"a502"};
    char *odd_space[]={"a 5"}, *prefix[]={"0xa5"};
    assert(!debug_hex_bytes(2,odd,out,1));
    assert(!debug_hex_bytes(1,odd_space,out,1));
    assert(!debug_hex_bytes(1,invalid,out,2));
    assert(!debug_hex_bytes(1,prefix,out,1));
    assert(!debug_hex_bytes(1,short_frame,out,4));
    assert(!debug_hex_bytes(1,joined,out,3));
    assert(!debug_hex_bytes(0,joined,out,4));
    return 0;
}
