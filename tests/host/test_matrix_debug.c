#include "matrix_debug.h"
#include <assert.h>
#include <string.h>
int main(void)
{
    matrix_model_t real;
    matrix_model_init(&real,0); matrix_model_stage(&real,MATRIX_BOOT_DONE,0);
    real.faults=MATRIX_PROTOCOL;
    matrix_model_t original=real;
    matrix_debug_t debug={.forced=MATRIX_BLUETOOTH}; uint8_t colors[25],expected[25];
    matrix_debug_frame(&debug,&real,500,colors);
    matrix_model_t forced=real; forced.faults |= MATRIX_BLUETOOTH;
    matrix_model_frame(&forced,500,expected);
    assert(!memcmp(colors,expected,25) && !memcmp(&real,&original,sizeof(real)));
    debug.forced=0; matrix_debug_frame(&debug,&real,500,colors);
    assert(colors[2]==MATRIX_ORANGE && colors[0]==MATRIX_OFF); /* Real fault survives. */
    debug.calibration=true; debug.started_ms=UINT32_MAX-499;
    const unsigned corners[]={0,4,24,20};
    const uint8_t palette[]={MATRIX_RED,MATRIX_GREEN,MATRIX_BLUE,MATRIX_WHITE};
    for (unsigned step=0;step<8;++step) {
        matrix_debug_frame(&debug,&real,debug.started_ms+step*1000,colors);
        for (unsigned i=0;i<25;++i) assert(colors[i]==(i==corners[step%4]?palette[step%4]:MATRIX_OFF));
    }
    debug=(matrix_debug_t){0}; matrix_debug_frame(&debug,&real,500,colors);
    assert(colors[2]==MATRIX_ORANGE && !memcmp(&real,&original,sizeof(real)));
    return 0;
}
