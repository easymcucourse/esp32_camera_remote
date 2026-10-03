#include "pad_player.h"

bool pad_player_enqueue(pad_player_t *p, const pad_sequence_t *s, uint32_t token)
{
    if (!s->count || s->count > PAD_ACT_MAX || p->count == 4) return false;
    p->jobs[(p->head+p->count)%4]=(pad_job_t){*s,token}; ++p->count; return true;
}
static void apply(pad_state_t *s, const pad_act_t *a)
{
    if (a->kind==PAD_PRESS) s->buttons |= a->mask;
    else if (a->kind==PAD_RELEASE) s->buttons &= ~a->mask;
    else if (a->kind==PAD_STICK) {
        if (a->side) { s->rx=(int8_t)a->x; s->ry=(int8_t)a->y; }
        else { s->lx=(int8_t)a->x; s->ly=(int8_t)a->y; }
    } else if (a->kind==PAD_TRIGGER) {
        if (a->side) s->r2=(uint8_t)a->value; else s->l2=(uint8_t)a->value;
    }
    /* Trigger bits follow analog thresholds, including after release all. */
    s->buttons &= ~((1u<<8)|(1u<<9));
    if (s->l2>=77) s->buttons |= 1u<<8;
    if (s->r2>=77) s->buttons |= 1u<<9;
}
void pad_player_tick(pad_player_t *p, uint32_t now, const pad_player_ops_t *ops)
{
    while (p->count) {
        pad_job_t *job=&p->jobs[p->head];
        if (!p->running) { p->running=true; p->action=0; p->started=p->due=now; }
        if ((int32_t)(now-p->due)<0) return;
        bool changed=false;
        while (p->action<job->sequence.count) {
            const pad_act_t *a=&job->sequence.actions[p->action++];
            if (a->kind==PAD_WAIT) {
                if (changed) ops->apply(ops->context,&p->state);
                changed=false; p->due+=a->value;
                if ((int32_t)(now-p->due)<0) return;
                continue; /* Late wakes don't accumulate a fresh delay per wait. */
            }
            apply(&p->state,a); changed=true;
        }
        if (changed) ops->apply(ops->context,&p->state);
        ops->done(ops->context,job->token,now-p->started,false);
        p->head=(p->head+1)%4; --p->count; p->running=false;
    }
}
void pad_player_cancel(pad_player_t *p, uint32_t now, const pad_player_ops_t *ops)
{
    while (p->count) {
        ops->done(ops->context,p->jobs[p->head].token,p->running?now-p->started:0,true);
        p->head=(p->head+1)%4; --p->count; p->running=false;
    }
    p->state=(pad_state_t){.battery=255}; ops->apply(ops->context,&p->state);
}
