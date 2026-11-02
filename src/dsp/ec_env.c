/*
 * ec_env.c -- SC EnvGen segments and the three envelope shapes the engine uses:
 *   - readerAmpEnv ADSR  (gate = timed window or held until noteOff)
 *   - readerAmpEnv AHR   (attack, hold, release; ignores the gate)
 *   - slice voice env    (ADSR shape with a release node, gate external; the
 *                         AHR form is ADSR with decay 0.0001 and sustain 1)
 * Curves are SC's: shape 'curve' -4 for A/D/R, 'sin' where the SynthDefs say so.
 */
#include "ec_internal.h"

void ec_seg_start(ec_seg_t *s, float end, float secs, int shape, float curve, float fs)
{
    double start = s->level;
    s->end = end;
    if (secs >= EC_ENV_INF_SECONDS * 0.1f) {   /* "infinite": stay where we are */
        s->counter = -1;
        s->shape = SEG_HOLD;
        return;
    }
    double c = (double)secs * (double)fs;
    if (c < 1.0) c = 1.0;
    if (c > 2.0e9) c = 2.0e9;
    s->counter = (int32_t)c;
    s->shape = shape;
    switch (shape) {
    case SEG_LIN:
        s->slope = (end - start) / (double)s->counter;
        break;
    case SEG_CURVE:
        if (fabs(curve) < 0.001) {
            s->shape = SEG_LIN;
            s->slope = (end - start) / (double)s->counter;
        } else {
            double a1 = (end - start) / (1.0 - exp((double)curve));
            s->a2 = start + a1;
            s->b1 = a1;
            s->grow = exp((double)curve / (double)s->counter);
        }
        break;
    case SEG_SIN: {
        double w = EC_PI / (double)s->counter;
        s->a2 = (end + start) * 0.5;
        s->b1 = 2.0 * cos(w);
        s->y1 = (end - start) * 0.5;
        s->y2 = s->y1 * sin(EC_PI * 0.5 - w);
        s->level = s->a2 - s->y1;
    } break;
    default:   /* hold: level stays, jumps to end when the time is up */
        break;
    }
}

void ec_env_reset(ec_env_t *e)
{
    memset(e, 0, sizeof *e);
    e->stage = ENV_IDLE;
    e->seg.counter = -1;
    e->gate_left = 0;
}

static void env_release(ec_env_t *e, const ec_envspec_t *sp, float fs)
{
    e->stage = ENV_REL;
    ec_seg_start(&e->seg, 0.f, sp->rel, SEG_CURVE, -4.f, fs);
}

void ec_env_trigger(ec_env_t *e, const ec_envspec_t *sp, int32_t gate_samples,
                    int legato, float fs)
{
    int32_t gl = gate_samples > 0 ? gate_samples : -1;
    if (legato && e->gate) {   /* portamento: the open window just extends */
        e->gate_left = gl;
        return;
    }
    e->gate = 1;
    e->gate_left = gl;
    e->stage = ENV_ATK;
    ec_seg_start(&e->seg, 1.f, sp->atk, SEG_CURVE, -4.f, fs);
}

void ec_env_gate_off(ec_env_t *e, const ec_envspec_t *sp, float fs)
{
    e->gate = 0;
    e->gate_left = 0;
    if (sp->mode == EC_ENV_AHR) return;      /* AHR ignores the gate */
    if (e->stage == ENV_ATK || e->stage == ENV_DEC || e->stage == ENV_SUS)
        env_release(e, sp, fs);
}

void ec_env_note_off(ec_env_t *e, const ec_envspec_t *sp, float fs)
{
    /* SetResetFF only gates an INDEFINITE note; a timed window ignores noteOff */
    if (e->gate && e->gate_left < 0) ec_env_gate_off(e, sp, fs);
}

void ec_env_next(ec_env_t *e, const ec_envspec_t *sp, float fs)
{
    switch (e->stage) {
    case ENV_ATK:
        if (sp->mode == EC_ENV_AHR) {
            e->stage = ENV_HOLD;
            ec_seg_start(&e->seg, 1.f, sp->hold, SEG_HOLD, 0.f, fs);
        } else {
            e->stage = ENV_DEC;
            ec_seg_start(&e->seg, sp->sus, sp->dec, SEG_CURVE, -4.f, fs);
        }
        break;
    case ENV_DEC:
        e->stage = ENV_SUS;
        e->seg.counter = -1;
        break;
    case ENV_HOLD:
        env_release(e, sp, fs);
        break;
    case ENV_REL:
        e->stage = ENV_DONE;
        e->seg.level = 0.0;
        e->seg.counter = -1;
        break;
    default:
        e->seg.counter = -1;
        break;
    }
}
