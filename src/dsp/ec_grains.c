/*
 * ec_grains.c -- ElasticatGrains, the particle granular engine. The emitter is a
 * silent cursor; every spawned grain reads the sample on its own, wrapping at
 * the buffer ends (never at the loop fences), so moving the region only moves
 * the emitter and in-flight grains never click. Spawn interval jittered +-30 %
 * against comb-filtering; Hann window from a LUT; cubic reads.
 */
#include "ec_internal.h"

void ec_grains_process(ec_grains_t *gs, const ec_sample_t *s, const float *hann,
                       double emit0, double emit_inc, double density, double cycle_samples,
                       double spread_frames, double base_rate, double speed_rand,
                       double dir_balance, float *L, float *R, int n)
{
    double fbuf = (double)s->frames;
    if (density < 1.0) density = 1.0;
    if (density > (double)(EC_MAX_GRAINS - 1)) density = (double)(EC_MAX_GRAINS - 1);
    if (cycle_samples < 4.0) cycle_samples = 4.0;
    double interval = cycle_samples / density;
    if (interval < 1.0) interval = 1.0;
    float inc = (float)(1.0 / cycle_samples);
    speed_rand = ec_clampd(speed_rand, 0.0, 1.0);
    double thresh = 2.0 * ec_clampd(dir_balance, 0.0, 1.0) - 1.0;
    double emit = emit0;

    for (int i = 0; i < n; ++i) {
        gs->spawn -= 1.0;
        if (gs->spawn <= 0.0) {
            gs->spawn += interval * (1.0 + 0.3 * (double)ec_frand2(&gs->rng));
            if (gs->n < EC_MAX_GRAINS) {
                ec_grain_t *g = &gs->g[gs->n++];
                double p = emit + (double)ec_frand2(&gs->rng) * spread_frames;
                p = fmod(p, fbuf);
                if (p < 0.0) p += fbuf;
                g->pos = p;
                double smul = 1.0 + speed_rand * (double)ec_frand2(&gs->rng);
                double dir = ((double)ec_frand2(&gs->rng) < thresh) ? 1.0 : -1.0;
                g->rate = base_rate * smul * dir;
                g->phase = 0.f;
                g->inc = inc;
            }
        }
        emit += emit_inc;
        if (emit >= fbuf) emit -= fbuf;

        float ml = 0.f, mr = 0.f, l, r;
        int k = 0;
        while (k < gs->n) {
            ec_grain_t *g = &gs->g[k];
            float win = ec_hann_lerp(hann, g->phase);
            ec_peek_wrap(s, g->pos, &l, &r);
            ml += l * win;
            mr += r * win;
            g->pos += g->rate;
            if (g->pos >= fbuf) g->pos -= fbuf;
            else if (g->pos < 0.0) g->pos += fbuf;
            g->phase += g->inc;
            if (g->phase >= 1.f) gs->g[k] = gs->g[--gs->n];
            else ++k;
        }
        L[i] = ml;
        R[i] = mr;
    }
}
