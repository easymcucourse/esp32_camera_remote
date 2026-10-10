#include "gimbal_proto_rs3.h"
#include <string.h>

/* DUML wire format: rs3mini-bridge and jdesbonnet/dji_rs3_control.
 * See docs/design/rs3-mini-protocol.md for pinned evidence and limitations. */
static uint8_t crc8(const uint8_t *p, size_t n)
{
    uint8_t crc=0x77; /* reflected 0xee */
    while (n--) { crc^=*p++; for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0x8c:0); }
    return crc;
}
static uint16_t crc16(const uint8_t *p, size_t n)
{
    uint16_t crc=0x3692; /* reflected 0x496c */
    while (n--) { crc^=*p++; for (unsigned i=0;i<8;++i) crc=(crc>>1)^((crc&1)?0x8408:0); }
    return crc;
}
static void put16(uint8_t *p, uint16_t value) { p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8); }
static uint16_t get16(const uint8_t *p) { return p[0]|((uint16_t)p[1]<<8); }
size_t rs3_frame(uint8_t *out, size_t cap, uint16_t seq, uint8_t receiver,
                 uint8_t flags, uint8_t set, uint8_t id, const uint8_t *payload, size_t size)
{
    if (!out || (!payload && size) || size>RS3_FRAME_MAX-13 || cap<size+13) return 0;
    size_t n=size+13;
    out[0]=0x55; out[1]=(uint8_t)n; out[2]=4|(uint8_t)(n>>8); out[3]=crc8(out,3);
    out[4]=2; out[5]=receiver; put16(out+6,seq); out[8]=flags; out[9]=set; out[10]=id;
    if (size) memcpy(out+11,payload,size);
    put16(out+n-2,crc16(out,n-2)); return n;
}
size_t rs3_stick(uint8_t *out, size_t cap, uint16_t seq, int16_t pan, int16_t tilt)
{
    if (pan < -400 || pan > 400 || tilt < -400 || tilt > 400) return 0;
    uint8_t payload[9]={0,4,0,4,0,4,0,0,2};
    put16(payload,(uint16_t)(1024+tilt)); put16(payload+4,(uint16_t)(1024+pan));
    return rs3_frame(out,cap,seq,4,0x40,4,1,payload,sizeof(payload));
}
size_t rs3_center(uint8_t *out, size_t cap, uint16_t seq)
{ const uint8_t p[]={0xfe,1}; return rs3_frame(out,cap,seq,4,0x40,4,0x4c,p,2); }
bool rs3_valid(const uint8_t *p, size_t size)
{
    return p && size>=13 && size<=RS3_FRAME_MAX && p[0]==0x55 && (p[2]>>2)==1 &&
        (size_t)(p[1]|((p[2]&3)<<8))==size && p[3]==crc8(p,3) && get16(p+size-2)==crc16(p,size-2);
}
void rs3_stream_feed(rs3_stream_t *s, const uint8_t *bytes, size_t size, rs3_frame_fn fn, void *context)
{
    if (!s || (!bytes && size) || !fn) return;
    for (size_t i=0;i<size;++i) {
        if (s->used==sizeof(s->bytes)) s->used=0;
        s->bytes[s->used++]=bytes[i];
        while (s->used) {
            if (s->bytes[0]!=0x55) { memmove(s->bytes,s->bytes+1,--s->used); continue; }
            if (s->used<4) break;
            size_t n=s->bytes[1]|((s->bytes[2]&3)<<8);
            if (n<13 || n>RS3_FRAME_MAX || (s->bytes[2]>>2)!=1 || s->bytes[3]!=crc8(s->bytes,3)) {
                memmove(s->bytes,s->bytes+1,--s->used); continue;
            }
            if (s->used<n) break;
            if (rs3_valid(s->bytes,n)) { fn(context,s->bytes,n); s->used-=n; memmove(s->bytes,s->bytes+n,s->used); }
            else { memmove(s->bytes,s->bytes+1,--s->used); }
        }
    }
}
bool rs3_battery(const uint8_t *p, size_t n, uint8_t *percent)
{
    /* Mini-specific observed 21-byte payload; don't accept other model layouts. */
    if (!percent || !rs3_valid(p,n) || p[4]!=0xe5 || p[5]!=2 || p[9]!=0x0d || p[10]!=2 || n!=34 || p[n-3]>100) return false;
    *percent=p[n-3]; return true;
}
bool rs3_pose_raw(const uint8_t *p, size_t n, rs3_pose_raw_t *pose)
{
    if (!pose || !rs3_valid(p,n) || (p[4]!=4 && p[4]!=0xe5) || p[5]!=2 ||
        p[9]!=4 || p[10]!=0x66 || n<14 || p[11]!=1) return false;
    rs3_pose_raw_t result={0}; unsigned seen=0;
    size_t at=12, end=n-2;
    while (at<end) {
        if (end-at<2) return false;
        uint8_t tag=p[at++], length=p[at++];
        if (length>end-at) return false;
        if (tag>=0x22 && tag<=0x24) {
            unsigned bit=1u<<(tag-0x22);
            if (length!=2 || (seen&bit)) return false;
            uint16_t raw=get16(p+at);
            int16_t value=(int16_t)(raw<=INT16_MAX?(int32_t)raw:(int32_t)raw-65536);
            if (tag==0x22) result.tilt=value;
            else if (tag==0x23) result.roll=value;
            else result.pan=value;
            seen|=bit;
        }
        at+=length;
    }
    if (seen!=7) return false;
    *pose=result; return true;
}
bool rs3_name(const char *name)
{ return name && (!strncmp(name,"DJI RS3 MINI-",13) || !strncmp(name,"DJI RS 3 Mini",13)); }
bool rs3_candidate(bool saved, const uint8_t target[6], const uint8_t address[6], const char *name)
{ return address && (saved ? target && !memcmp(target,address,6) : rs3_name(name)); }
