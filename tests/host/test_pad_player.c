#include "pad_player.h"
#include <assert.h>
#include <stdio.h>
static unsigned applied, done_count, cancelled_count;
static pad_state_t state;
static uint32_t last_token,last_elapsed;
static void apply(void *ctx,const pad_state_t *next) { (void)ctx; ++applied; state=*next; }
static void done(void *ctx,uint32_t token,uint32_t elapsed,bool cancelled)
{ (void)ctx; ++done_count; cancelled_count+=cancelled; last_token=token; last_elapsed=elapsed; }
int main(void)
{
    pad_player_ops_t ops={apply,done,NULL};
    pad_player_t p={.state={.connected=true,.battery=7}};
    char *a[]={"seq","hold","rt;","wait","100;","trigger","rt","full;","wait","100;","release","all"};
    pad_sequence_t s; const char *error;
    assert(pad_cmd_parse(12,a,&s,&error));
    assert(pad_player_enqueue(&p,&s,10));
    pad_player_tick(&p,UINT32_MAX-49,&ops); assert(state.r2==153 && (state.buttons&(1u<<9)) && !done_count);
    pad_player_tick(&p,49,&ops); assert(state.r2==153); /* 99 ms across wrap. */
    pad_player_tick(&p,50,&ops); assert(state.r2==242);
    pad_player_tick(&p,150,&ops); assert(!state.buttons && !state.r2 && state.battery==7 && done_count==1);
    assert(last_token==10 && last_elapsed==200 && !p.count);
    for (unsigned i=0;i<4;++i) assert(pad_player_enqueue(&p,&s,i+20));
    assert(!pad_player_enqueue(&p,&s,99));
    pad_player_tick(&p,1000,&ops); assert(state.r2==153);
    pad_player_cancel(&p,1040,&ops);
    assert(done_count==5 && cancelled_count==4 && !p.count && !state.connected && !state.buttons && !state.r2 && state.battery==255);
    unsigned previous=applied; pad_player_tick(&p,2000,&ops); assert(previous==applied); /* No stale actions survive exit. */
    char *b[]={"seq","stick","r","127","-128;","hold","a;","trigger","lt","half"};
    assert(pad_cmd_parse(10,b,&s,&error)); p.state.connected=true;
    assert(pad_player_enqueue(&p,&s,100)); pad_player_tick(&p,2001,&ops);
    assert(applied==previous+1 && state.rx==127 && state.ry==-128 && state.l2==153 && state.buttons==((1u<<14)|(1u<<8)));
    char *late[]={"seq","wait","1;","wait","1;","wait","1;","release","all"};
    assert(pad_cmd_parse(9,late,&s,&error)); assert(pad_player_enqueue(&p,&s,101));
    pad_player_tick(&p,3000,&ops); unsigned count=done_count;
    pad_player_tick(&p,3010,&ops); assert(done_count==count+1 && last_elapsed==10);
    puts("Pad scheduler deadlines, wraparound, queue capacity, coalescing and cancellation passed");
}
