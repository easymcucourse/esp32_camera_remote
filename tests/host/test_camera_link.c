#include <assert.h>
#include <stdio.h>
#include "camera_link.h"
int main(void) {
    const uint8_t saved[6]={1,2,3,4,5,6}, changed[6]={1,2,3,4,5,7};
    assert(camera_candidate_allowed(false,saved,changed));
    assert(camera_candidate_allowed(true,saved,saved));
    assert(!camera_candidate_allowed(true,saved,changed));
    assert(camera_select_candidate(0)==-1);
    assert(camera_select_candidate(1)==0 && camera_select_candidate(8)==3);
    assert(camera_select_candidate(9)==-2);
    assert(camera_select_candidate(0x80000000u)==31);
    const unsigned expected[]={1,2,4,8,16,30,30};
    for (unsigned i=0;i<7;++i) assert(camera_retry_delay(i)==expected[i]);
    assert(camera_retry_delay(1000)==30);
    puts("camera link policy tests passed"); return 0;
}
