#include "ble_advertisement.h"
#include <string.h>
#include <ctype.h>

static bool named_gamepad(const char *name)
{
    char lower[33];
    unsigned i=0;
    for (;name[i] && i<32;++i) lower[i]=(char)tolower((unsigned char)name[i]);
    lower[i]=0;
    return strstr(lower,"xbox") || strstr(lower,"gamepad") || strstr(lower,"joystick") ||
           strstr(lower,"8bitdo ultimate 2");
}
bool ble_advertisement_parse(ble_advertisement_t *out,const uint8_t *data,size_t size)
{
    if (!out || (!data && size)) return false;
    ble_advertisement_t next=*out;
    for (size_t at=0;at<size;) {
        size_t length=data[at++];
        if (!length) break;
        if (length>size-at) return false;
        uint8_t type=data[at];const uint8_t *value=data+at+1;size_t n=length-1;
        if (type==2 || type==3) {
            if (n%2) return false;
            for (size_t i=0;i<n;i+=2) if (value[i]==0x12 && value[i+1]==0x18) next.hid=true;
        } else if (type==0x19) {
            if (n!=2) return false;
            uint16_t appearance=value[0]|((uint16_t)value[1]<<8);
            if (appearance==0x03c3 || appearance==0x03c4) next.gamepad=true;
        } else if (type==8 || type==9) {
            size_t count=n<32?n:32;memcpy(next.name,value,count);next.name[count]=0;
            if (named_gamepad(next.name)) next.gamepad=true;
        }
        at+=length;
    }
    *out=next;return true;
}
