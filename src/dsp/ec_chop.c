/*
 * ec_chop.c -- warp mode 13 "chopped" (\elasticatChopSync): the Digitakt-style
 * timestretch. A loop-synced linear ramp sweeps the read START across the
 * region; a step clock (one grain per SLEN steps) latches the ramp and starts a
 * natural-pitch read there, gated by an AHR grain envelope (sin / lin / sin).
 * Port note: the SynthDef retriggered ONE read (a jump if the previous grain was
 * still sounding); here a new grain takes the second head and the previous one
 * releases from where it is, so a full GATE no longer clicks.
 */
#include "ec_internal.h"

void ec_chop_reset(ec_chop_t *c, double phase)
{
    c->ramp = phase - floor(phase);
    c->imp = 0.0;
    c->primed = 1;
}

static void head_release(ec_chop_head_t *h, float rel, float fs)
{
    if (h->stage == 0 || h->stage == 3) return;
    h->stage = 3;
    ec_seg_start(&h->env, 0.f, rel, SEG_SIN, 0.f, fs);
}

void ec_chop_process(ec_dsp_t *d, ec_track_t *t, double cps, double pitch_rate,
                     double run, double dir, float *L, float *R, int n)
{
    const ec_track_params_t *p = &t->p;
    ec_chop_t *c = &t->ch;
    float fs = d->fs;
    double frames = t->frames;
    double slen = ec_clampd(p->chop_slice_len, 0.05, 64.0);
    double wr = t->rate_lag;
    double step_freq = ((double)d->bpm * wr / 15.0) / slen;
    if (step_freq < 0.01) step_freq = 0.01;
    double imp_inc = step_freq * run / fs;
    double ramp_inc = cps * dir * run / fs;
    float atk = ec_clampf(p->chop_attack, 0.0002f, 0.5f);
    float rel = ec_clampf(p->chop_release, 0.0002f, 0.5f);
    float gate = ec_clampf(p->chop_hold, 0.01f, 1.f);
    float sus = gate * (float)(1.0 / step_freq) - atk - rel;
    if (sus < 0.0005f) sus = 0.0005f;
    double lo = t->lo, span = t->hi - t->lo;

    for (int i = 0; i < n; ++i) {
        int trig = 0;
        if (run > 0.0) {
            if (c->primed) { trig = 1; c->primed = 0; }
            else { c->imp += imp_inc; if (c->imp >= 1.0) { c->imp -= 1.0; trig = 1; } }
        }
        if (trig) {
            ec_chop_head_t *old = &c->h[c->active];
            head_release(old, rel, fs);
            c->active ^= 1;
            ec_chop_head_t *h = &c->h[c->active];
            h->pos = (lo + c->ramp * span) * frames;
            h->rate = pitch_rate;
            h->env.level = 0.0;
            h->stage = 1;
            ec_seg_start(&h->env, 1.f, atk, SEG_SIN, 0.f, fs);
        }
        c->ramp += ramp_inc;
        if (c->ramp >= 1.0) c->ramp -= 1.0;
        else if (c->ramp < 0.0) c->ramp += 1.0;

        float ml = 0.f, mr = 0.f, l, r;
        for (int k = 0; k < 2; ++k) {
            ec_chop_head_t *h = &c->h[k];
            if (h->stage == 0) continue;
            float e = ec_seg_tick(&h->env);
            if (h->env.counter == 0) {
                if (h->stage == 1) { h->stage = 2; ec_seg_start(&h->env, 1.f, sus, SEG_HOLD, 0.f, fs); }
                else if (h->stage == 2) { h->stage = 3; ec_seg_start(&h->env, 0.f, rel, SEG_SIN, 0.f, fs); }
                else { h->stage = 0; h->env.level = 0.0; h->env.counter = -1; }
            }
            ec_peek_wrap(&t->smp, h->pos, &l, &r);
            ml += l * e;
            mr += r * e;
            h->pos += h->rate;
        }
        L[i] = ml * EC_S16_SCALE;
        R[i] = mr * EC_S16_SCALE;
    }
}
