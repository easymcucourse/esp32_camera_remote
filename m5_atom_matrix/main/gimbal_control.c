#include "gimbal_control.h"

static int16_t curve(int raw, unsigned span)
{
    if (raw > 127) raw = 127;
    if (raw < -127) raw = -127;
    int magnitude = raw < 0 ? -raw : raw;
    if (magnitude <= 13) return 0;
    int value = (int)(span * (unsigned)((magnitude-13)*(magnitude-13)) / (114u*114u));
    return (int16_t)(raw < 0 ? -value : value);
}
gimbal_output_t gimbal_control_step(gimbal_control_t *s, const gimbal_tuning_t *cfg,
                                   const gimbal_input_t *in, bool ready, uint32_t now)
{
    gimbal_output_t out = {0};
    unsigned span = cfg->span > 400 ? 400 : cfg->span;
    unsigned tilt_span = cfg->tilt_span ? cfg->tilt_span : span;
    if (tilt_span > 400) tilt_span = 400;
    int16_t pan = curve((int)in->x-cfg->offset_x, span);
    int16_t tilt = curve((int)in->y-cfg->offset_y, tilt_span);
    if (cfg->invert_y) tilt = -tilt;
    bool centered = !pan && !tilt;
    bool fresh = in->connected && (uint32_t)(now-in->report_ms) < 200;
    if (!ready) { *s = (gimbal_control_t){0}; return out; }
    if (!s->linked || s->epoch != in->epoch) {
        *s = (gimbal_control_t){.linked=true,.epoch=in->epoch,.l3=in->l3,.sent=true,.sent_ms=now};
        out.action = GIMBAL_STOP; return out;
    }
    if (!fresh) {
        s->armed=false; s->l3=in->l3;
        if (s->moving || s->centering) out.action=GIMBAL_STOP;
        s->moving=s->centering=false;
    } else if (!s->armed) {
        s->l3=in->l3;
        if (centered && !in->l3) s->armed=true;
    } else {
        bool press = in->l3 && !s->l3;
        s->l3=in->l3;
        if (press) {
            out.action=GIMBAL_CENTER; s->centering=true; s->moving=false; s->center_ms=now;
        } else if (s->centering && ((uint32_t)(now-s->center_ms)>=5000 || (!in->l3 && !centered))) {
            out.action=GIMBAL_STOP; s->centering=false; s->moving=false;
        } else if (!s->centering && centered && s->moving) {
            out.action=GIMBAL_STOP; s->moving=false;
        } else if (!s->centering && (!s->sent || (uint32_t)(now-s->sent_ms)>=200)) {
            out.action=centered?GIMBAL_STOP:GIMBAL_MOVE;
            out.pan=pan; out.tilt=tilt; s->moving=!centered;
        }
    }
    if (out.action!=GIMBAL_NONE) { s->sent=true; s->sent_ms=now; }
    return out;
}
