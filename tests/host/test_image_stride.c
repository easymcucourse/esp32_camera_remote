#include "image_stride.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    uint16_t guarded[13*47+2], original[13*31];
    for (size_t width=1; width<=31; ++width)
        for (size_t stride=width; stride<=47; ++stride)
            for (size_t height=1; height<=13; ++height) {
                size_t capacity=stride*height;
                for (size_t i=0; i<sizeof(guarded)/sizeof(*guarded); ++i) guarded[i]=0xbeef;
                for (size_t i=0; i<width*height; ++i) original[i]=guarded[i+1]=(uint16_t)(i*37+5);
                assert(image_stride_expand(guarded+1,capacity,width,height,stride));
                for (size_t y=0; y<height; ++y)
                    for (size_t x=0; x<width; ++x)
                        assert(guarded[1+y*stride+x]==original[y*width+x]);
                assert(guarded[0]==0xbeef && guarded[capacity+1]==0xbeef);
            }
    uint16_t small[]={1,2,3,4,5,6};
    assert(!image_stride_expand(NULL,6,2,2,3));
    assert(!image_stride_expand(small,6,0,2,3));
    assert(!image_stride_expand(small,6,2,0,3));
    assert(!image_stride_expand(small,6,3,2,2));
    assert(!image_stride_expand(small,5,2,2,3));
    assert(!image_stride_expand(small,SIZE_MAX,2,SIZE_MAX,3));
    assert(!image_stride_expand(small,SIZE_MAX,2,1,SIZE_MAX));
    for (unsigned i=0; i<6; ++i) assert(small[i]==i+1);
    puts("image stride: overlapping/disjoint rows, bounds and guards passed");
    return 0;
}
