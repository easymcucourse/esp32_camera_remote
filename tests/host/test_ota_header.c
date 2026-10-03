#include "ota_header.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    uint8_t p[288]={0};ota_header_info_t out;
    assert(ota_header_timestamp("Oct  3 2026","11:50:00")>ota_header_timestamp("Sep 30 2026","23:59:59"));
    assert(ota_header_timestamp("Jan  1 2027","00:00:00")>ota_header_timestamp("Dec 31 2026","23:59:59"));
    assert(!ota_header_timestamp("Oct  3 2026","25:00:00"));
    p[0]=0xe9;p[1]=4;p[12]=9;p[23]=1;p[29]=1;p[32]=0x32;p[33]=0x54;p[34]=0xcd;p[35]=0xab;
    strcpy((char*)p+80,"esp32_camera_remote");strcpy((char*)p+48,"v1");
    assert(!ota_header_check(p,288,1024,2048,9,"esp32_camera_remote",&out));assert(!strcmp(out.version,"v1"));
    assert(ota_header_check(p,287,1024,2048,9,out.project,&out));
    assert(ota_header_check(p,288,1024,1023,9,out.project,&out));
    assert(ota_header_check(p,288,300,2048,9,out.project,&out));
    p[12]=0;assert(ota_header_check(p,288,1024,2048,9,out.project,&out));p[12]=9;
    p[23]=0;assert(ota_header_check(p,288,1024,2048,9,out.project,&out));p[23]=1;
    assert(ota_header_check(p,288,1024,2048,9,"another",&out));
    p[32]=0;assert(ota_header_check(p,288,1024,2048,9,out.project,&out));p[32]=0x32;
    memset(p+48,'x',32);assert(ota_header_check(p,288,1024,2048,9,out.project,&out));return 0;
}
