/*
 * ec_click.c -- the metronome click (see ec_click.h). Each sound = up to two damped resonators started at their
 * peak (a cosine onset: the click is audible from its first sample) + an optional high-passed noise burst, an
 * optional soft-square drive, an attack ramp and a linear release to the end. Peaks are normalised per sound and
 * accent at init, so every sound sits at the same loudness.
 */
#include <math.h>
#include <string.h>
#include "ec_click.h"

const char *const ec_click_names[EC_CLICK_N] = { "WOOD", "BEEP", "CLICK", "COWBELL" };

typedef struct {
    float f0, acc_ratio;   /* first mode (Hz), the accent's pitch ratio */
    float tau0;            /* its decay (s) */
    float f1_ratio, tau1, g1;   /* second mode: ratio to f0, decay, gain (0 = none) */
    float nz_g, nz_tau, hp_hz;  /* noise burst */
    float sq;              /* soft square drive (0 = off) */
    float len_ms, atk_ms, rel_ms;
} click_spec_t;

static const click_spec_t SPECS[EC_CLICK_N] = {
    /* WOOD */    { 1150.f, 1.35f, 0.022f, 2.62f, 0.010f, 0.45f, 0.50f, 0.0015f, 2000.f, 0.f,  70.f, 0.f, 3.f },
    /* BEEP */    {  880.f, 2.00f, 1.000f, 0.00f, 0.000f, 0.00f, 0.00f, 0.0000f,    0.f, 0.f,  50.f, 2.f, 8.f },
    /* CLICK */   { 3200.f, 1.35f, 0.004f, 0.00f, 0.000f, 0.00f, 1.00f, 0.0012f, 4000.f, 0.f,  25.f, 0.f, 2.f },
    /* COWBELL */ {  560.f, 1.33f, 0.050f, 1.48f, 0.045f, 0.80f, 0.00f, 0.0000f,    0.f, 3.f, 110.f, 1.f, 10.f },
};

#define CLICK_PEAK_ACCENT 0.63f    /* -4 dBFS */
#define CLICK_PEAK_PLAIN  0.38f    /* -8.4 dBFS */
#define TWO_PI 6.28318530717958647692

static void mode_start(ec_click_mode_t *m, float f, float tau, float g, float sr)
{
    if (g <= 0.f || f <= 0.f || f >= 0.45f * sr) { memset(m, 0, sizeof *m); return; }
    double w = TWO_PI * (double)f / (double)sr;
    double r = exp(-1.0 / ((double)tau * (double)sr));
    m->c = (float)(2.0 * r * cos(w));
    m->r2 = (float)(r * r);
    /* y[n] = g r^n cos(w n): seed y[-1], y[-2] */
    m->y1 = (float)((double)g * cos(w) / r);
    m->y2 = (float)((double)g * cos(2.0 * w) / (r * r));
    m->g = g;
}

static void voice_start(ec_click_t *c, int sound, int accent, float amp)
{
    const click_spec_t *s = &SPECS[sound];
    float sr = c->sr;
    float f0 = s->f0 * (accent ? s->acc_ratio : 1.f);
    mode_start(&c->m[0], f0, s->tau0, 1.f, sr);
    mode_start(&c->m[1], f0 * s->f1_ratio, s->tau1, s->g1, sr);
    c->nz_g = s->nz_g * (accent ? 1.2f : 1.f);
    c->nz_d = s->nz_tau > 0.f ? expf(-1.f / (s->nz_tau * sr)) : 0.f;
    c->hp_a = s->hp_hz > 0.f ? expf(-(float)TWO_PI * s->hp_hz / sr) : 0.f;
    c->hp_x = c->hp_y = 0.f;
    c->sq = s->sq;
    c->len = (uint32_t)(s->len_ms * 0.001f * sr);
    if (c->len < 8) c->len = 8;
    c->atk = (uint32_t)(s->atk_ms * 0.001f * sr);
    c->rel = (uint32_t)(s->rel_ms * 0.001f * sr);
    if (c->rel < 1) c->rel = 1;
    if (c->rel > c->len) c->rel = c->len;
    c->t = 0;
    c->rng = 0x1234567u;
    c->amp = amp;
    c->active = 1;
}

/* one raw sample (before amp) */
static float voice_tick(ec_click_t *c)
{
    float y = 0.f;
    for (int k = 0; k < 2; ++k) {
        ec_click_mode_t *m = &c->m[k];
        if (m->g == 0.f) continue;
        float v = m->c * m->y1 - m->r2 * m->y2;
        m->y2 = m->y1;
        m->y1 = v;
        y += v;
    }
    if (c->sq > 0.f) { float x = y * c->sq; y = x / (1.f + fabsf(x)); }
    if (c->nz_g > 1e-6f) {
        c->rng = c->rng * 1664525u + 1013904223u;
        float n = (float)(int32_t)c->rng * (1.f / 2147483648.f);
        float h = c->hp_a > 0.f ? c->hp_a * (c->hp_y + n - c->hp_x) : n;
        c->hp_x = n;
        c->hp_y = h;
        y += h * c->nz_g;
        c->nz_g *= c->nz_d;
    }
    uint32_t t = c->t++;
    float e = 1.f;
    if (t < c->atk) e = (float)(t + 1) / (float)c->atk;
    uint32_t left = c->len - t;   /* t < len */
    if (left <= c->rel) e *= (float)left / (float)c->rel;
    if (c->t >= c->len) c->active = 0;
    return y * e;
}

/* 1 / each sound's raw peak (per accent) at this rate, into c->norm */
static void build_norms(ec_click_t *c)
{
    ec_click_t tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.sr = c->sr;
    for (int s = 0; s < EC_CLICK_N; ++s)
        for (int a = 0; a < 2; ++a) {
            voice_start(&tmp, s, a, 1.f);
            float pk = 0.f;
            while (tmp.active) { float v = fabsf(voice_tick(&tmp)); if (v > pk) pk = v; }
            c->norm[s][a] = pk > 1e-6f ? 1.f / pk : 0.f;
        }
}

void ec_click_init(ec_click_t *c, float sr)
{
    memset(c, 0, sizeof *c);
    c->sr = sr > 1000.f ? sr : 44100.f;
    build_norms(c);
}

void ec_click_trigger(ec_click_t *c, int accent, int sound, float level)
{
    if (!c) return;
    if (!(level > 0.f)) return;
    if (level > 1.f) level = 1.f;
    if (sound < 0 || sound >= EC_CLICK_N) sound = EC_CLICK_WOOD;
    c->pend = 1;
    c->p_sound = sound;
    c->p_accent = accent ? 1 : 0;
    c->p_level = level;
}

static inline int16_t sat_add(int16_t a, int32_t b)
{
    int32_t v = (int32_t)a + b;
    return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
}

void ec_click_mix(ec_click_t *c, int16_t *stereo, uint32_t frames, uint32_t offset)
{
    if (!c || !stereo) return;
    /* an onset past this buffer waits for the next mix (whose own offset places it) */
    int start = c->pend && offset < frames;
    if (!c->active && !start) return;
    for (uint32_t i = 0; i < frames; ++i) {
        if (start && i == offset) {
            float pk = c->p_accent ? CLICK_PEAK_ACCENT : CLICK_PEAK_PLAIN;
            voice_start(c, c->p_sound, c->p_accent, c->p_level * pk * c->norm[c->p_sound][c->p_accent] * 32767.f);
            c->pend = 0;
            start = 0;
        }
        if (!c->active) {
            if (!start) break;
            continue;
        }
        float v = voice_tick(c) * c->amp;
        int32_t s = (int32_t)lrintf(v);
        stereo[2 * i] = sat_add(stereo[2 * i], s);
        stereo[2 * i + 1] = sat_add(stereo[2 * i + 1], s);
    }
}

int ec_click_busy(const ec_click_t *c)
{
    return c && (c->active || c->pend);
}
