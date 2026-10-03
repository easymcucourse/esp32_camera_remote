#include "pad_cmd.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static pad_sequence_t parse(char *line, bool success)
{
    char *argv[64]; int argc=0;
    char *word=strtok(line," ");
    while (word) { argv[argc++]=word; word=strtok(NULL," "); }
    pad_sequence_t out={.count=99}; const char *error=NULL;
    assert(pad_cmd_parse(argc,argv,&out,&error)==success);
    if (!success) { assert(error && out.count==99); } else assert(!error);
    return out;
}
int main(void)
{
    const char *aliases[][2]={{"select","share"},{"start","options"},{"a","cross"},{"b","circle"},
        {"x","square"},{"y","triangle"},{"lb","l1"},{"rb","r1"},{"lt","l2"},{"rt","r2"}};
    for (unsigned i=0;i<sizeof(aliases)/sizeof(aliases[0]);++i)
        assert(pad_cmd_button_mask(aliases[i][0])==pad_cmd_button_mask(aliases[i][1]));
    assert(pad_cmd_button_mask("START")==8 && !pad_cmd_button_mask("foo"));
    char tap[]="tap A+b"; pad_sequence_t s=parse(tap,true);
    assert(s.count==3 && s.actions[0].mask==((1u<<14)|(1u<<13)) && s.duration_ms==100);
    char trigger[]="seq hold rt; wait 300; trigger rt full; wait 200; release all";
    s=parse(trigger,true); assert(s.count==9 && s.duration_ms==500 && s.actions[0].value==PAD_HALF);
    assert(s.actions[2].value==PAD_FULL && !s.camera_warning);
    char shoot[]="shoot"; s=parse(shoot,true); assert(s.count==7 && s.duration_ms==600 && s.camera_warning);
    char record[]="record"; s=parse(record,true); assert(s.count==5 && s.duration_ms==400 && s.camera_warning);
    char stick[]="stick l -128 127"; s=parse(stick,true); assert(s.actions[0].x==-128 && s.actions[0].y==127);
    const char *invalid[]={"tap a 0","tap a 10001","hold a+","hold +a","tap absent","trigger rt 256",
        "trigger rt -1","trigger rt 99999999999","stick r 128 0","stick l -129 0","seq seq tap a",
        "seq hold a;","seq ; hold a","release all a","wait -1","wait +10","tap a 1x","tap","seq"};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) { char line[256]; strcpy(line,invalid[i]); parse(line,false); }
    char many[256]="seq ";
    for (unsigned i=0;i<11;++i) strcat(many,i?";tap a 1":"tap a 1");
    parse(many,false); /* 33 expanded actions, no partial result. */
    puts("Pad aliases, sequence expansion, warning, bounds and atomic rejection passed");
}
