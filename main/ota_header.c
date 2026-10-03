#include "ota_header.h"
#include <string.h>
#include <stdio.h>
uint64_t ota_header_timestamp(const char *date,const char *time)
{
    if (!date || !time || strlen(date)!=11 || strlen(time)!=8) return 0;
    const char *months="JanFebMarAprMayJunJulAugSepOctNovDec";unsigned month=0,day,year,hour,minute,second;
    for (unsigned i=0;i<12;++i) if (!memcmp(date,months+i*3,3)) month=i+1;
    if (!month || sscanf(date+4,"%u %u",&day,&year)!=2 || sscanf(time,"%u:%u:%u",&hour,&minute,&second)!=3 ||
        !day || day>31 || year<1970 || year>9999 || hour>23 || minute>59 || second>59) return 0;
    return (((((uint64_t)year*100+month)*100+day)*100+hour)*100+minute)*100+second;
}
static uint32_t le32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
const char *ota_header_check(const uint8_t *p,size_t prefix,size_t total,size_t capacity,
                             uint16_t chip,const char *project,ota_header_info_t *out)
{
    if (!p || !project || !out || prefix<OTA_PREFIX_SIZE || total<OTA_PREFIX_SIZE+32) return "image too short";
    if (total>capacity) return "image too large";
    if (p[0]!=0xe9 || !p[1] || p[1]>16) return "not an ESP firmware image";
    if (((uint16_t)p[12]|(uint16_t)p[13]<<8)!=chip) return "firmware is for another chip";
    if (p[23]!=1) return "image must include SHA256";
    if (le32(p+28)<256 || le32(p+28)>total-32) return "invalid first segment";
    if (le32(p+32)!=0xabcd5432) return "missing app description";
    const unsigned offsets[]={48,80,112,128},sizes[]={32,32,16,16};
    for (unsigned i=0;i<4;++i) if (!memchr(p+offsets[i],0,sizes[i])) return "unterminated app description";
    if (strcmp((const char*)p+80,project)) return "firmware is for another project";
    ota_header_info_t info;
    memcpy(info.version,p+48,32);memcpy(info.project,p+80,32);memcpy(info.time,p+112,16);memcpy(info.date,p+128,16);
    *out=info;return NULL;
}
