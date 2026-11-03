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

/* ---------------------------------------------------------------- the grain snapshot (0.6.0) */

typedef struct {
    ec_grain_view_t *out;
    int n;
    const float *hann;
    double inv_f, fbuf;
    float best_age;       /* the newest grain's age so far (samples) */
    int newest;           /* its index in out[] (-1: not kept) */
    ec_grain_view_t cand; /* the newest grain itself, in case it is not among the loudest */
    int have_cand;
} gview_t;

/* one grain: phase 0..1 of its life, inc per sample, read position / rate in frames */
static void gview_add(gview_t *v, double pos, double rate, float phase, float inc)
{
    if (!(inc > 0.f)) return;
    ec_grain_view_t g;
    double p = fmod(pos, v->fbuf);
    if (p < 0.0) p += v->fbuf;
    g.pos = (float)(p * v->inv_f);
    double len = fabs(rate) / (double)inc * v->inv_f;
    g.len = (float)(len > 1.0 ? 1.0 : len);
    float ph = ec_clampf(phase, 0.f, 0.9999f);
    g.level = ec_clampf(ec_hann_lerp(v->hann, ph), 0.f, 1.f);
    g.newest = 0;
    float age = phase / inc;   /* inc is fixed at spawn: phase / inc = samples since the spawn */
    int is_new = !v->have_cand || age < v->best_age;
    if (is_new) { v->best_age = age; v->cand = g; v->have_cand = 1; v->newest = -1; }
    if (v->n < EC_GRAIN_VIEW_MAX) {
        if (is_new) v->newest = v->n;
        v->out[v->n++] = g;
        return;
    }
    /* full: replace the quietest kept grain that is not the newest */
    int q = -1;
    for (int k = 0; k < v->n; ++k)
        if (k != v->newest && (q < 0 || v->out[k].level < v->out[q].level)) q = k;
    if (q >= 0 && (is_new || g.level > v->out[q].level)) {
        v->out[q] = g;
        if (is_new) v->newest = q;
    }
}

int ec_dsp_grains(ec_dsp_t *d, int track, ec_grain_view_t *out)
{
    if (!d || !out || track < 0 || track >= EC_TRACKS) return 0;
    const ec_track_t *t = &d->tr[track];
    if (!t->has_smp || t->idle || t->smp.frames < 1) return 0;
    gview_t v;
    v.out = out; v.n = 0; v.hann = d->hann;
    v.fbuf = (double)t->smp.frames;
    v.inv_f = 1.0 / v.fbuf;
    v.best_age = 0.f; v.newest = -1; v.have_cand = 0;
    if (t->reader_on) {
        if (t->fam == FAM_GRAINS || t->prev_fam == FAM_GRAINS)
            for (int k = 0; k < t->gr.n; ++k) {
                const ec_grain_t *g = &t->gr.g[k];
                gview_add(&v, g->pos, g->rate, g->phase, g->inc);
            }
        if ((t->fam == FAM_READER || t->prev_fam == FAM_READER) &&
            (t->w3.stage == W3_ST_OLA || t->w3.prev_stage == W3_ST_OLA))
            for (int k = 0; k < t->w3.ong; ++k) {
                const ec_olag_t *g = &t->w3.og[k];
                gview_add(&v, g->pos, g->rate, g->ph, g->inc);
            }
    }
    for (int i = 0; i < EC_VOICE_POOL; ++i) {
        const ec_voice_t *vo = &d->v[i];
        if (!vo->used || vo->track != track || vo->kind != 1) continue;
        for (int k = 0; k < vo->ng; ++k)
            gview_add(&v, vo->g[k].pos, vo->g[k].rate, vo->g[k].phase, vo->g[k].inc);
    }
    if (v.have_cand) {
        if (v.newest < 0) {   /* dropped for a louder one: it takes the quietest place */
            int q = 0;
            for (int k = 1; k < v.n; ++k) if (out[k].level < out[q].level) q = k;
            out[q] = v.cand;
            v.newest = q;
        }
        out[v.newest].newest = 1;
    }
    return v.n;
}
