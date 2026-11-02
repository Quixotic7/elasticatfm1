/*
 * ec_reader.c -- ElasticatReader (lib/ugens/src/ElasticatUGens.cpp), the
 * softcut-model click-free reader used by tape and tempo_varispeed:
 *   1. absolute-frame reads: lo/hi are fences, moving them never moves the read;
 *   2. one queued equal-power crossfade between exactly two heads; a jump that
 *      arrives mid-fade waits for the running fade to finish;
 *   3. the loop seam is crossfaded too (old head reads on past the fence while
 *      the new head fades in at the other end).
 * One deliberate refinement: a seam wrap carries the overshoot (new head =
 * lo + (p - hi)) instead of restarting exactly at lo, so a tempo-fitted loop
 * keeps sample-exact time over any number of passes (the UGen dropped up to one
 * frame per pass).
 */
#include "ec_internal.h"

void ec_reader_init(ec_reader_t *r)
{
    memset(r, 0, sizeof *r);
    r->state[0] = r->state[1] = RH_STOP;
}

static void cut_to_phase(ec_reader_t *r, double pos)
{
    int na = r->active ^ 1;
    if (r->state[r->active] != RH_STOP) r->state[r->active] = RH_FADEOUT;
    r->state[na] = RH_FADEIN;
    r->phase[na] = pos;
    r->hactive[r->active] = 0;
    r->hactive[na] = 1;
    r->active = na;
}

void ec_reader_cut(ec_reader_t *r, double pos)
{
    int s = r->state[r->active];
    if (s == RH_FADEIN || s == RH_FADEOUT) { r->queued_pos = pos; r->queued = 1; }
    else cut_to_phase(r, pos);
}

void ec_reader_process(ec_reader_t *r, const ec_sample_t *s, double lo, double hi,
                       double rate, double start_pos, float fade_inc,
                       float *L, float *R, int n)
{
    if (hi < lo + 1.0) hi = lo + 1.0;
    double w = hi - lo;

    /* auto-start: rate != 0 with nothing sounding fades the active head in */
    if (rate != 0.0 && r->state[0] == RH_STOP && r->state[1] == RH_STOP) {
        r->state[r->active] = RH_FADEIN;
        r->phase[r->active] = start_pos;
        r->fade[r->active] = 0.f;
        r->hactive[r->active] = 1;
    }

    for (int i = 0; i < n; ++i) {
        float ml = 0.f, mr = 0.f, l, rr;
        for (int h = 0; h < 2; ++h) {
            if (r->state[h] == RH_STOP) continue;
            ec_peek_clamp(s, r->phase[h], &l, &rr);
            if (r->state[h] == RH_PLAY) { ml += l; mr += rr; }
            else { float g = ec_sin_hp(r->fade[h]); ml += l * g; mr += rr * g; }
        }
        L[i] = ml * EC_S16_SCALE;
        R[i] = mr * EC_S16_SCALE;

        /* advance; only the ACTIVE head checks its fence */
        for (int h = 0; h < 2; ++h) {
            if (r->state[h] == RH_STOP) continue;
            double p0 = r->phase[h];
            double p = p0 + rate;
            if (r->hactive[h] && (p > hi || p < lo)) {
                int natural = (rate >= 0.0) ? (p0 <= hi && p > hi) : (p0 >= lo && p < lo);
                if (natural) { r->queued = 2; }
                else { r->queued = 1; r->queued_pos = (rate >= 0.0) ? lo : hi; }
            }
            r->phase[h] = p;
        }

        for (int h = 0; h < 2; ++h) {
            if (r->state[h] == RH_FADEIN) {
                r->fade[h] += fade_inc;
                if (r->fade[h] > 1.f) { r->fade[h] = 1.f; r->state[h] = RH_PLAY; }
            } else if (r->state[h] == RH_FADEOUT) {
                r->fade[h] -= fade_inc;
                if (r->fade[h] < 0.f) { r->fade[h] = 0.f; r->state[h] = RH_STOP; }
            }
        }

        int as = r->state[r->active];
        if (as != RH_FADEIN && as != RH_FADEOUT && r->queued) {
            double target;
            if (r->queued == 2) {           /* seam wrap: carry the overshoot */
                double p = r->phase[r->active];
                if (rate >= 0.0) {
                    double o = p - hi;
                    if (o < 0.0) o = 0.0;
                    if (o >= w) o = fmod(o, w);
                    target = lo + o;
                } else {
                    double o = lo - p;
                    if (o < 0.0) o = 0.0;
                    if (o >= w) o = fmod(o, w);
                    target = hi - o;
                }
            } else {
                target = r->queued_pos;
            }
            cut_to_phase(r, target);
            r->queued = 0;
        }
    }
}
