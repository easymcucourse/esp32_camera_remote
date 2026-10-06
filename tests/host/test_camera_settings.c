#include "../support/legacy/ui_camera_vendor_codes.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "camera_settings.h"
#include "sony_props.h"
#include "ptpip_packet.h"
static unsigned calls;
static uint32_t current;
static void visit(void *ctx, uint16_t code, uint32_t value) {
    (void)ctx; assert(code == 0xd211); ++calls; current = value;
}
static void check(unsigned i, uint32_t v, const char *expected) {
    char text[40]; camera_extra_format(i, v, text, sizeof(text)); assert(!strcmp(text, expected));
}
static unsigned seen;
static void capture_visit(void *ctx, uint16_t code, uint32_t value) {
    (void)ctx;
    for (unsigned i=0; i<CAMERA_EXTRA_COUNT; ++i) {
        if (camera_extra_codes[i]==code) {
            static const uint32_t expected[CAMERA_EXTRA_COUNT] = {2,0,0x8000,1,2,1,5500,0xc0,0xc0};
            assert(value==expected[i]); assert(!(seen & (1u<<i))); seen |= 1u<<i;
        }
    }
}
int main(int argc, char **argv) {
    uint8_t b[18] = {0}; put32(b, 1); b[8]=0x11; b[9]=0xd2; b[10]=2;
    b[12]=1; b[13]=1; b[14]=1; b[15]=2;
    assert(sony_parse_scalar_properties(b, 17, visit, NULL));
    assert(calls==1 && current==2); calls=0;
    for (size_t n=0; n<17; ++n) assert(!sony_parse_scalar_properties(b,n,visit,NULL));
    assert(!sony_parse_scalar_properties(b,18,visit,NULL)); assert(calls==0);
    check(0, 2, "ASPECT 16:9"); check(1, 0x18012, "DRIVE LO");
    check(2, 0x8000, "EFFECT OFF"); check(3, 0x11, "DRO LV1");
    check(4, 0x103, "AF AREA SPOT L"); check(6, 5500, "WB TEMP 5500K");
    check(7, 0xc0, "WB AB RAW 0X000000C0");
    check(1, 0x12345678, "DRIVE 0X12345678");
    check(5, UINT32_MAX, "WL FLASH --");
    check(5, 1, "WL FLASH 0X00000001");
    if (argc==2) {
        FILE *f=fopen(argv[1], "rb"); assert(f);
        uint8_t sample[16384]; size_t n=fread(sample,1,sizeof(sample),f);
        assert(feof(f)); fclose(f);
        assert(sony_parse_scalar_properties(sample,n,capture_visit,NULL));
        assert(seen==(1u<<CAMERA_EXTRA_COUNT)-1); seen=0;
        for (size_t truncated=0; truncated<n; ++truncated) {
            assert(!sony_parse_scalar_properties(sample,truncated,capture_visit,NULL));
            assert(seen==0);
        }
        printf("capture scalar properties validated: %zu bytes, nine extra fields\n",n);
    }
    puts("camera settings tests passed"); return 0;
}

