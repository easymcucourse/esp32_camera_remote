#include "ultimate2_report.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    uint8_t map[113];memcpy(map,ultimate2_report_map,sizeof(map));
    assert(ultimate2_map_matches(map,sizeof(map)));
    for (unsigned i=0;i<sizeof(map);++i) {
        assert(!ultimate2_map_matches(map,i));map[i]^=1;
        assert(!ultimate2_map_matches(map,sizeof(map)));map[i]^=1;
    }
    uint8_t p[34]={15,127,127,127,127};ds4_state_t s;
    assert(ultimate2_parse_report(p,33,&s));assert(s.connected && !s.buttons && s.rx==-1 && s.battery==255);
    for (unsigned i=0;i<33;++i) assert(!ultimate2_parse_report(p,i,&s));
    assert(!ultimate2_parse_report(p,34,&s));
    p[1]=0;p[2]=255;p[3]=255;p[4]=0;p[5]=230;p[6]=77;
    assert(ultimate2_parse_report(p,33,&s));assert(s.lx==-128 && s.ly==127 && s.rx==127 && s.ry==-128 && s.r2==230 && s.l2==77);
    const unsigned bits[15]={13,14,31,12,15,31,10,11,8,9,0,3,16,1,2};
    for (unsigned i=0;i<24;++i) {
        p[7]=p[8]=p[9]=0;p[7+i/8]=1u<<(i%8);
        assert(ultimate2_parse_report(p,33,&s));
        assert(s.buttons==(i<15 && bits[i]!=31 ? 1u<<bits[i] : 0));
    }
    p[7]=p[8]=p[9]=0;
    const unsigned hats[8]={16,48,32,96,64,192,128,144};
    for (unsigned h=0;h<8;++h) {p[0]=h;assert(ultimate2_parse_report(p,33,&s));assert(s.buttons==hats[h]);}
    for (unsigned h=8;h<15;++h) {p[0]=h;ds4_state_t old=s;assert(!ultimate2_parse_report(p,33,&s));assert(!memcmp(&old,&s,sizeof(s)));}
    return 0;
}
