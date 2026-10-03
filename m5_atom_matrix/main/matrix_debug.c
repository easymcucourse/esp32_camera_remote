#include "matrix_debug.h"
#include <string.h>
void matrix_debug_frame(const matrix_debug_t *debug, const matrix_model_t *model,
                        uint32_t now, uint8_t colors[25])
{
    if (debug->calibration) {
        static const uint8_t corners[]={0,4,24,20};
        static const uint8_t color[]={MATRIX_RED,MATRIX_GREEN,MATRIX_BLUE,MATRIX_WHITE};
        unsigned step=((uint32_t)(now-debug->started_ms)/1000)%4;
        memset(colors,MATRIX_OFF,25); colors[corners[step]]=color[step];
    } else {
        matrix_model_t snapshot=*model;
        snapshot.faults |= debug->forced;
        matrix_model_frame(&snapshot,now,colors);
    }
}
