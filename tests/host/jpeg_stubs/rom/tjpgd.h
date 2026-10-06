#pragma once
typedef unsigned int UINT;
typedef unsigned char BYTE;
typedef enum { JDR_OK, JDR_INTR, JDR_INP, JDR_MEM1, JDR_MEM2, JDR_PAR, JDR_FMT1 } JRESULT;
typedef struct { unsigned short left,right,top,bottom; } JRECT;
typedef struct { unsigned short width,height; void *device; } JDEC;
JRESULT jd_prepare(JDEC *,UINT (*)(JDEC *,BYTE *,UINT),void *,UINT,void *);
JRESULT jd_decomp(JDEC *,UINT (*)(JDEC *,void *,JRECT *),BYTE);
