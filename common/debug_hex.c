#include "debug_hex.h"
static int nibble(char c)
{
    if (c>='0' && c<='9') return c-'0';
    if (c>='a' && c<='f') return c-'a'+10;
    if (c>='A' && c<='F') return c-'A'+10;
    return -1;
}
bool debug_hex_bytes(int argc,char **argv,uint8_t *out,size_t expected)
{
    size_t length=0; int high=-1;
    for (int i=0;i<argc;++i) {
      for (const char *p=argv[i];*p;++p) {
        if (*p==' ' || *p=='\t') { if (high>=0) return false; continue; }
        int n=nibble(*p); if (n<0) return false;
        if (high<0) high=n;
        else { if (length==expected) return false; out[length++]=(uint8_t)(high*16+n); high=-1; }
      }
      if (high>=0) return false;
    }
    return high<0 && length==expected;
}
