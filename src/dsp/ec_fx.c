/*
 * ec_fx.c -- E4 FX machines: C99 port of the 18 \elasticatFx* SynthDefs
 * (Engine_Elasticat.sc ~1636-2120). One ec_fx_t = one slot.
 *
 * Structure
 *   ec_fx_process splits a call into chunks of <= EC_FX_KR frames. Per chunk:
 *   control values (LFOs, Lags, swept coefficients) once, audio per sample.
 *   A machine writes its WET signal to a scratch pair; the shared tail then
 *   applies fxMixBlend (XFade2 equal power) for the machines that have a MIX,
 *   and the click-free swap fade (machine output <-> slot dry / silence).
 *   Derived constants (maps, filter coefficients) are recomputed only when the
 *   params change (memcmp against a cached copy), with cheap exp2/log2/tan
 *   approximations: no libm in the per-call path.
 *
 * Memory
 *   Lines are carved from the caller's slot memory on every (re)start. Nothing
 *   is cleared: each line counts the samples written since its start and older
 *   ages read as 0 (SC's "checked" delay helpers do the same), so a swap costs
 *   O(1) instead of a 1.5 MB memset.
 *
 * Silence
 *   A sleep detector (input and output below -150 dBFS for longer than the
 *   machine's memory) stops processing and outputs exact zeros; the machine
 *   restarts clean when input returns. That ends every tail at exact 0 and
 *   keeps subnormals out; per-chunk flushes of filter states and per-sample
 *   flushes on recirculating writes cover the half-silent cases.
 *
 * Deviations from the SC are listed in src/dsp/README.md (FX section).
 */
#include <math.h>
#include <string.h>
#include "ec_fx.h"
#include "ec_internal.h"

#define FX_PI      3.14159265358979f
#define FX_QUIET   1e-6f                 /* -120 dBFS: sleep threshold */
#define FX_LOG2_001  (-9.96578428f)      /* log2(0.001) (Lag, Decay, AllpassC) */
#define FX_LOG2_01   (-3.32192809f)      /* log2(0.1)   (Amplitude, Compander) */

/* ------------------------------------------------------------ fast math */

static inline float fx_exp2(float x)
{
    if (x < -126.f) x = -126.f;
    if (x > 126.f) x = 126.f;
    int xi = (int)x;
    if ((float)xi > x) --xi;
    float f = x - (float)xi;
    float p = 1.f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f +
              f * (0.00961813f + f * (0.00133336f + f * 0.00015404f)))));
    union { float f; uint32_t u; } v;
    v.u = (uint32_t)(xi + 127) << 23;
    return p * v.f;
}

static inline float fx_log2(float x)               /* x > 0, normal */
{
    union { float f; uint32_t u; } v;
    v.f = x;
    int e = (int)((v.u >> 23) & 255u) - 127;
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;
    float m = v.f;
    if (m > 1.41421356f) { m *= 0.5f; ++e; }
    float t = (m - 1.f) / (m + 1.f), t2 = t * t;
    return (float)e + t * (2.88539008f + t2 * (0.96179669f + t2 * (0.57707801f + t2 * 0.41219858f)));
}

/* tan for 0 <= x <= ~1.42 (Pade 5/4; 0.1 % at 1.41) */
static inline float fx_tan(float x)
{
    float x2 = x * x;
    return x * (105.f - 10.f * x2) / (105.f - x2 * (45.f - x2));
}

/* sin(2 pi ph), any ph; |err| < 4e-6 */
static inline float fx_sin2pi(float ph)
{
    ph -= (float)(int)ph;
    if (ph < 0.f) ph += 1.f;
    if (ph > 0.5f) ph -= 1.f;
    if (ph > 0.25f) ph = 0.5f - ph;
    else if (ph < -0.25f) ph = -0.5f - ph;
    float x = ph * 6.28318531f, x2 = x * x;
    return x * (1.f - x2 * (1.f / 6.f - x2 * (1.f / 120.f - x2 * (1.f / 5040.f - x2 * (1.f / 362880.f)))));
}

static inline float fx_clip(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
/* SC linlin / linexp, clip: \minmax */
static inline float fx_linlin(float x, float a, float b, float lo, float hi)
{
    x = fx_clip(x, a, b);
    return lo + (hi - lo) * (x - a) / (b - a);
}
static inline float fx_linexp(float x, float a, float b, float lo, float hi)
{
    if (!(x > a)) return lo;              /* also NaN */
    if (x >= b) return hi;
    return lo * fx_exp2(fx_log2(hi / lo) * (x - a) / (b - a));
}
static inline float fx_dbamp(float db) { return fx_exp2(db * 0.16609640f); }
static inline float fx_ampdb(float a) { return 6.02059991f * fx_log2(a); }

static inline float fz(float v) { return (v < 1e-20f && v > -1e-20f) ? 0.f : v; }
#define FL(x) do { if ((x) < 1e-15f && (x) > -1e-15f) (x) = 0.f; } while (0)

/* SC softclip: linear to 0.5, then (|x| - 0.25) / x */
static inline float sc_softclip(float x)
{
    float a = fabsf(x);
    return a <= 0.5f ? x : (a - 0.25f) / x;
}
/* SC fold2(x, 1) */
static inline float sc_fold1(float x)
{
    if (x >= -1.f && x <= 1.f) return x;
    float c = x + 1.f;
    c -= 4.f * floorf(c * 0.25f);
    if (c >= 2.f) c = 4.f - c;
    return c - 1.f;
}

/* XFade2 equal-power gains for pos -1..1 (a = left/dry, b = right/wet) */
static void fx_xfade(float t01, float *ga, float *gb)
{
    if (t01 <= 0.f) { *ga = 1.f; *gb = 0.f; return; }
    if (t01 >= 1.f) { *ga = 0.f; *gb = 1.f; return; }
    *gb = fx_sin2pi(t01 * 0.25f);
    *ga = fx_sin2pi((1.f - t01) * 0.25f);
}

#if defined(__GNUC__) || defined(__clang__)
#define FX_CTZ(x) __builtin_ctz(x)
#else
static int FX_CTZ(uint32_t x) { int n = 0; while (!(x & 1u)) { x >>= 1; ++n; } return n; }
#endif

/* ------------------------------------------------------------ SC filters */

static void lpf_coef(ec_fxbq_t *c, float hz, float fs)
{
    hz = fx_clip(hz, 5.f, 0.45f * fs);
    float C = 1.f / fx_tan(FX_PI * hz / fs);
    float C2 = C * C, s = 1.41421356f * C;
    c->a0 = 1.f / (1.f + s + C2);
    c->b1 = -2.f * (1.f - C2) * c->a0;
    c->b2 = -(1.f - s + C2) * c->a0;
}
static void hpf_coef(ec_fxbq_t *c, float hz, float fs)
{
    hz = fx_clip(hz, 5.f, 0.45f * fs);
    float C = fx_tan(FX_PI * hz / fs);
    float C2 = C * C, s = 1.41421356f * C;
    c->a0 = 1.f / (1.f + s + C2);
    c->b1 = 2.f * (1.f - C2) * c->a0;
    c->b2 = -(1.f - s + C2) * c->a0;
}
static inline float lpf_tick(const ec_fxbq_t *c, ec_fxbqs_t *s, float x)
{
    float y0 = x + c->b1 * s->y1 + c->b2 * s->y2;
    float o = c->a0 * (y0 + 2.f * s->y1 + s->y2);
    s->y2 = s->y1; s->y1 = y0;
    return o;
}
static inline float hpf_tick(const ec_fxbq_t *c, ec_fxbqs_t *s, float x)
{
    float y0 = x + c->b1 * s->y1 + c->b2 * s->y2;
    float o = c->a0 * (y0 - 2.f * s->y1 + s->y2);
    s->y2 = s->y1; s->y1 = y0;
    return o;
}
static inline float bpf_tick(const ec_fxbq_t *c, ec_fxbqs_t *s, float x)
{
    float y0 = x + c->b1 * s->y1 + c->b2 * s->y2;
    float o = c->a0 * (y0 - s->y2);
    s->y2 = s->y1; s->y1 = y0;
    return o;
}
static inline void bq_flush(ec_fxbqs_t *s) { FL(s->y1); FL(s->y2); }

/* ------------------------------------------------------------ lines */

static void line_carve(ec_fxline_t *l, float *mem, uint32_t *off, int32_t len)
{
    if (*off + (uint32_t)len + 3u > EC_FX_MEM_FLOATS) len = (int32_t)(EC_FX_MEM_FLOATS - *off) - 3;
    l->buf = mem + *off;
    l->len = len;
    l->w = 0;
    l->filled = 0;
    *off += (uint32_t)len + 3u;
}
static inline void line_write(ec_fxline_t *l, float x)
{
    int32_t w = l->w + 1;
    if (w >= l->len) w = 0;
    l->w = w;
    l->buf[w] = x;
    if (w < 3) l->buf[l->len + w] = x;
    if (l->filled < l->len) l->filled++;
}
static inline float line_age(const ec_fxline_t *l, int32_t a)
{
    if (a < 0 || a >= l->filled) return 0.f;
    int32_t j = l->w - a;
    if (j < 0) j += l->len;
    return l->buf[j];
}
static float line_cubic_cold(const ec_fxline_t *l, int32_t id, float fr)
{
    return ec_hermite(fr, line_age(l, id - 1), line_age(l, id), line_age(l, id + 1), line_age(l, id + 2));
}
/* DelayC read at age d (>= 1) from the most recent write (SC cubicinterp, d0 newest) */
static inline float line_cubic(const ec_fxline_t *l, float d)
{
    int32_t id = (int32_t)d;
    float fr = d - (float)id;
    if (id + 2 >= l->filled) return line_cubic_cold(l, id, fr);
    int32_t j = l->w - id - 2;
    if (j < 0) j += l->len;
    const float *b = l->buf + j;
    return ec_hermite(fr, b[3], b[2], b[1], b[0]);
}
static inline float line_maxd(const ec_fxline_t *l) { return (float)(l->len - 4); }

/* ------------------------------------------------------------ kr noise */

static void n2_init(ec_fxn2_t *n, uint32_t *rng)
{
    n->a = ec_frand2(rng); n->b = ec_frand2(rng); n->c = ec_frand2(rng); n->ph = 0.f;
}
/* LFNoise2: quadratic (midpoint-to-midpoint Bezier through each random value) */
static inline float n2_tick(ec_fxn2_t *n, float inc, uint32_t *rng)
{
    n->ph += inc;
    while (n->ph >= 1.f) { n->ph -= 1.f; n->a = n->b; n->b = n->c; n->c = ec_frand2(rng); }
    float t = n->ph, u = 1.f - t;
    return u * u * 0.5f * (n->a + n->b) + 2.f * t * u * n->b + t * t * 0.5f * (n->b + n->c);
}
static inline float n1_tick(ec_fxn1_t *n, float inc, uint32_t *rng)
{
    n->ph += inc;
    while (n->ph >= 1.f) { n->ph -= 1.f; n->a = n->b; n->b = ec_frand2(rng); }
    return n->a + (n->b - n->a) * n->ph;
}

/* SC PinkNoise (Voss-McCartney, 16 dice + 1 white, 19-bit values) */
static void pink_init(ec_fxpink_t *p, uint32_t *rng)
{
    p->total = 0; p->counter = 0;
    for (int i = 0; i < 16; ++i) { p->dice[i] = ec_xorshift(rng) >> 13; p->total += p->dice[i]; }
}
static inline float pink_tick(ec_fxpink_t *p, uint32_t *rng)
{
    uint32_t c = p->counter;
    int k = c ? (FX_CTZ(c) & 15) : 0;
    uint32_t nr = ec_xorshift(rng) >> 13;
    p->total += nr - p->dice[k];
    p->dice[k] = nr;
    p->counter = c + 1u;
    uint32_t w = ec_xorshift(rng) >> 13;
    return (float)(p->total + w) * (1.f / 4194304.f) - 1.f;
}

/* ------------------------------------------------------------ SC Limiter */

static void lim_carve(ec_fxlim_t *l, float *mem, uint32_t *off, int32_t size)
{
    l->in = mem + *off; l->mid = l->in + size; l->out = l->mid + size;
    *off += 3u * (uint32_t)size;
    l->size = size; l->pos = 0; l->flips = 0;
    l->level = 1.f; l->tgt = 1.f; l->slope = 0.f; l->prevmax = 0.f; l->curmax = 0.f;
}
static void lim_flip(ec_fxlim_t *l, float amp)
{
    float mx = l->prevmax > l->curmax ? l->prevmax : l->curmax;
    l->prevmax = l->curmax;
    l->curmax = 0.f;
    float next = mx > amp ? amp / mx : 1.f;
    l->level = l->tgt;                    /* exact end of the last ramp (no drift) */
    l->tgt = next;
    l->slope = (next - l->level) / (float)l->size;
    float *t = l->out; l->out = l->mid; l->mid = l->in; l->in = t;
    if (l->flips < 2) l->flips++;
}
static inline float lim_tick(ec_fxlim_t *l, float x, float amp)
{
    int32_t p = l->pos;
    float y = l->flips >= 2 ? l->level * l->out[p] : 0.f;
    l->in[p] = x;
    l->level += l->slope;
    float a = fabsf(x);
    if (a > l->curmax) l->curmax = a;
    if (++p >= l->size) { lim_flip(l, amp); p = 0; }
    l->pos = p;
    return y;
}

/* Amplitude / Compander peak follower (SC: log(0.1) coefficients) */
static inline float follow(float prev, float v, float cc, float rc)
{
    return v < prev ? v + (prev - v) * rc : v + (prev - v) * cc;
}

/* ------------------------------------------------------------ tables */

static const char *const FX_NAMES[EC_FX_COUNT] = {
    "NONE", "DELAY", "REVERB", "LOFI", "COMP", "DESTROY", "ECHO", "BLACKHOLE", "CHORUS",
    "FLANGER", "PHASER", "DJ EQ", "DUCK", "TAPE ECHO", "CASSETTE", "MOTION", "RINGS", "LIMIT"
};
/* fxMixBlend users */
static const uint8_t FX_HAS_MIX[EC_FX_COUNT + 1] = {
    0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 0
};
/* FreeVerb2 line lengths (SC, fixed in samples): L allpass 0..3, L combs
 * 4..11, R allpass 12..15, R combs 16..23 (stereo spread 23) */
static const int16_t FV_LEN[24] = {
    225, 341, 441, 556, 1617, 1557, 1491, 1422, 1277, 1116, 1188, 1356,
    248, 364, 464, 579, 1640, 1580, 1514, 1445, 1300, 1139, 1211, 1379
};
static const float BH_T[4] = {0.013f, 0.021f, 0.031f, 0.043f};

/* ------------------------------------------------------------ API: params */

void ec_fx_params_default(ec_fx_params_t *p)
{
    const float k = 1.f / 127.f;
    memset(p, 0, sizeof *p);
    p->machine = EC_FX_NONE;
    p->mix = 64 * k; p->drive = 0.f;
    p->delay_beats = 1.f; p->delay_feedback = 38 * k; p->delay_tone = 1.f;
    p->reverb_size = 64 * k; p->reverb_damp = 64 * k;
    p->lofi_bits = 24.f; p->lofi_rate = 48000.f;
    p->comp_thresh = 38 * k; p->comp_ratio = 25 * k; p->comp_attack = 13 * k;
    p->comp_release = 38 * k; p->comp_makeup = 0.f;
    p->destroy_tone = 1.f; p->destroy_level = 70 * k;
    p->echo_beats = 1.f; p->echo_offset = 0.f; p->echo_mode = 0; p->echo_tone = 1.f; p->echo_wobble = 0.f;
    p->bh_gravity = 64 * k; p->bh_mod = 25 * k; p->bh_low = 13 * k; p->bh_predelay = 13 * k;
    p->modfx_rate = 38 * k; p->modfx_depth = 38 * k; p->modfx_tone = 102 * k; p->chorus_width = 64 * k;
    p->flanger_feedback = 0.f;
    p->phaser_center = 64 * k; p->phaser_stages = 3; p->phaser_feedback = 25 * k;
    p->eq_low = p->eq_mid = p->eq_high = 0.f; p->eq_xlow = 38 * k; p->eq_xhi = 89 * k;
    p->duck_amount = 76 * k; p->duck_sens = 64 * k; p->duck_attack = 25 * k; p->duck_release = 51 * k;
    p->tape_wow = 25 * k; p->tape_flutter = 15 * k; p->tape_sat = 30 * k;
    p->cass_noise = 10 * k; p->cass_crackle = 0.f; p->cass_drop = 0.f; p->cass_tone = 80 * k;
    p->motion_width = 0.f; p->motion_beats = 1.f; p->motion_trem = 0.f; p->motion_pan = 0.f; p->motion_shape = 0;
    p->rings_mode = 0; p->rings_freq = 0.5f; p->rings_fine = 0.f; p->rings_feedback = 0.f;
    p->limit_gain = 0.f; p->limit_ceil = 124 * k; p->limit_release = 38 * k;
}

const char *ec_fx_name(int m)
{
    return (m >= 1 && m <= EC_FX_COUNT) ? FX_NAMES[m - 1] : "?";
}

/* ------------------------------------------------------------ derive */

static int32_t limit_size(const ec_fx_t *fx)
{
    float dur = fx_linexp(fx->pc.limit_release, 0.f, 1.f, 0.002f, 0.1f);
    int32_t s = (int32_t)ceilf(dur * fx->fs);
    return s < 1 ? 1 : s;
}

static void derive(ec_fx_t *fx)
{
    const ec_fx_params_t *p = &fx->pc;
    const float fs = fx->fs;
    ec_fxm_t *u = &fx->u;
    if (FX_HAS_MIX[fx->running]) fx_xfade(fx_clip(p->mix, 0.f, 1.f), &fx->gd_t, &fx->gw_t);
    else { fx->gd_t = 0.f; fx->gw_t = 1.f; }

    switch (fx->running) {
    case EC_FX_DELAY:
        lpf_coef(&u->dl.lp, fx_linexp(fx_clip(p->delay_tone, 0.f, 1.f), 0.001f, 1.f, 220.f, 18000.f), fs);
        u->dl.fbg = fx_clip(p->delay_feedback, 0.f, 0.95f);
        break;
    case EC_FX_REVERB: {
        float room = fx_clip(p->reverb_size, 0.f, 1.f), damp = fx_clip(p->reverb_damp, 0.f, 1.f);
        u->rv.fb = 0.7f + 0.28f * room;
        u->rv.d1 = 0.4f * damp;
        u->rv.d2 = 1.f - u->rv.d1;
    } break;
    case EC_FX_LOFI: {
        u->lf.inc = fx_clip(p->lofi_rate, 500.f, 24000.f) / fs;
        float bits = fx_clip(p->lofi_bits, 1.f, 24.f);
        u->lf.step = fx_exp2(1.f - bits);
        u->lf.inv = 1.f / u->lf.step;
    } break;
    case EC_FX_COMP: {
        ec_fxm_comp_t *c = &u->cp;
        c->thresh = fx_linlin(p->comp_thresh, 0.f, 1.f, 0.02f, 1.f);
        c->slope = fx_linexp(fx_clip(p->comp_ratio, 0.f, 1.f), 0.001f, 1.f, 1.f, 0.05f);
        float atk = fx_linexp(fx_clip(p->comp_attack, 0.f, 1.f), 0.001f, 1.f, 0.0005f, 0.1f);
        float rel = fx_linexp(fx_clip(p->comp_release, 0.f, 1.f), 0.001f, 1.f, 0.02f, 0.8f);
        c->makeup = fx_linlin(p->comp_makeup, 0.f, 1.f, 1.f, 4.f);
        c->cc = fx_exp2(FX_LOG2_01 / (atk * fs));
        c->rc = fx_exp2(FX_LOG2_01 / (rel * fs));
        c->mc = fx_exp2(FX_LOG2_01 / (0.01f * fs));
    } break;
    case EC_FX_DESTROY: {
        ec_fxm_destroy_t *d = &u->ds;
        d->warb = fx_clip(p->destroy_warble, 0.f, 1.f);
        d->nrate = fx_linexp(d->warb, 0.001f, 1.f, 0.5f, 9.f);
        d->driv = fx_clip(p->drive, 0.f, 1.f);
        d->dk = 1.f + d->driv * 24.f; d->dofs = d->driv * 0.15f;
        fx_xfade(d->driv, &d->dga, &d->dgb);
        float fo = fx_clip(p->destroy_fold, 0.f, 1.f);
        d->fk = 1.f + fo * 6.f; fx_xfade(fo, &d->fga, &d->fgb);
        float sa = fx_clip(p->destroy_sat, 0.f, 1.f);
        d->sk = 1.f + sa * 14.f; fx_xfade(sa, &d->sga, &d->sgb);
        float cr = fx_clip(p->destroy_crush, 0.f, 1.f);
        d->cstep = fx_exp2(1.f - fx_linlin(cr, 0.f, 1.f, 24.f, 2.f));
        d->cinv = 1.f / d->cstep; fx_xfade(cr, &d->cga, &d->cgb);
        float sr = fx_clip(p->destroy_srr, 0.f, 1.f);
        d->sinc = fx_linexp(sr, 0.f, 1.f, 48000.f, 500.f) / fs;
        fx_xfade(sr, &d->sga2, &d->sgb2);
        lpf_coef(&d->lp, fx_linexp(fx_clip(p->destroy_tone, 0.f, 1.f), 0.f, 1.f, 400.f, 18000.f), fs);
        d->lvl = fx_linlin(p->destroy_level, 0.f, 1.f, 0.2f, 2.f);
    } break;
    case EC_FX_ECHO: {
        ec_fxm_echo_t *e = &u->ec;
        e->offt = fx_linexp(fx_clip(p->echo_offset, -1.f, 1.f), -1.f, 1.f, 0.5f, 2.f);
        lpf_coef(&e->lp, fx_linexp(fx_clip(p->echo_tone, 0.f, 1.f), 0.f, 1.f, 300.f, 18000.f), fs);
        e->wob = fx_clip(p->echo_wobble, 0.f, 1.f);
        e->fbg = fx_clip(p->delay_feedback, 0.f, 0.95f);
        e->mode = p->echo_mode >= 1 ? 1 : 0;
    } break;
    case EC_FX_BLACKHOLE: {
        ec_fxm_bh_t *b = &u->bh;
        int32_t pres = (int32_t)(fx_linlin(p->bh_predelay, 0.f, 1.f, 0.f, 0.2f) * fs);
        b->pres = pres > fx->pre_len - 1 ? fx->pre_len - 1 : pres;
        b->size = fx_linlin(p->reverb_size, 0.f, 1.f, 0.5f, 1.6f);
        float g = fx_linlin(p->bh_gravity, 0.f, 1.f, 0.7f, 1.f);
        b->fbg = g > 0.9995f ? 0.9995f : g;
        lpf_coef(&b->lp, fx_linexp(fx_clip(p->reverb_damp, 0.f, 1.f), 0.f, 1.f, 16000.f, 700.f), fs);
        hpf_coef(&b->hp, fx_linexp(fx_clip(p->bh_low, 0.f, 1.f), 0.f, 1.f, 20.f, 600.f), fs);
        b->mod = fx_clip(p->bh_mod, 0.f, 1.f);
        b->decay = 3.5f * b->size;
    } break;
    case EC_FX_CHORUS: {
        ec_fxm_mod_t *m = &u->md;
        m->rate = fx_linexp(fx_clip(p->modfx_rate, 0.f, 1.f), 0.001f, 1.f, 0.05f, 8.f);
        m->depth = fx_linlin(p->modfx_depth, 0.f, 1.f, 0.f, 0.012f);
        m->wofs = fx_clip(p->chorus_width, 0.f, 1.f) * 0.5f;
        lpf_coef(&m->lp, fx_linexp(fx_clip(p->modfx_tone, 0.f, 1.f), 0.f, 1.f, 400.f, 18000.f), fs);
    } break;
    case EC_FX_FLANGER: {
        ec_fxm_mod_t *m = &u->md;
        m->rate = fx_linexp(fx_clip(p->modfx_rate, 0.f, 1.f), 0.001f, 1.f, 0.02f, 4.f);
        m->depth = fx_linlin(p->modfx_depth, 0.f, 1.f, 0.0002f, 0.006f);
        m->fbamt = fx_clip(p->flanger_feedback, -1.f, 1.f) * 0.9f;
        lpf_coef(&m->lp, fx_linexp(fx_clip(p->modfx_tone, 0.f, 1.f), 0.f, 1.f, 400.f, 18000.f), fs);
    } break;
    case EC_FX_PHASER: {
        ec_fxm_phaser_t *h = &u->ph;
        h->rate = fx_linexp(fx_clip(p->modfx_rate, 0.f, 1.f), 0.001f, 1.f, 0.02f, 4.f);
        float depth = fx_clip(p->modfx_depth, 0.f, 1.f);
        float ctr = fx_linexp(fx_clip(p->phaser_center, 0.f, 1.f), 0.f, 1.f, 100.f, 4000.f);
        h->fbamt = fx_clip(p->phaser_feedback, 0.f, 1.f) * 0.9f;
        float lo = ctr * (1.f - depth * 0.7f), hi = ctr * (1.f + depth * 1.5f);
        h->lo = lo < 40.f ? 40.f : lo;
        h->hi = hi > 12000.f ? 12000.f : hi;
        h->sel = ec_clampi(p->phaser_stages, 0, 7);
    } break;
    case EC_FX_DJ_EQ: {
        ec_fxm_eq_t *q = &u->eq;
        lpf_coef(&q->lp, fx_linexp(fx_clip(p->eq_xlow, 0.f, 1.f), 0.f, 1.f, 80.f, 500.f), fs);
        hpf_coef(&q->hp, fx_linexp(fx_clip(p->eq_xhi, 0.f, 1.f), 0.f, 1.f, 1000.f, 8000.f), fs);
        q->gl = fx_clip(1.f + fx_clip(p->eq_low, -1.f, 1.f), 0.f, 2.f);
        q->gm = fx_clip(1.f + fx_clip(p->eq_mid, -1.f, 1.f), 0.f, 2.f);
        q->gh = fx_clip(1.f + fx_clip(p->eq_high, -1.f, 1.f), 0.f, 2.f);
    } break;
    case EC_FX_DUCK: {
        ec_fxm_duck_t *k = &u->dk;
        k->amt = fx_clip(p->duck_amount, 0.f, 1.f);
        k->sens = fx_linexp(fx_clip(p->duck_sens, 0.f, 1.f), 0.f, 1.f, 0.5f, 8.f);
        float atk = fx_linexp(fx_clip(p->duck_attack, 0.f, 1.f), 0.f, 1.f, 0.001f, 0.05f);
        float rel = fx_linexp(fx_clip(p->duck_release, 0.f, 1.f), 0.f, 1.f, 0.05f, 1.2f);
        k->cc = fx_exp2(FX_LOG2_01 / (atk * fs));
        k->rc = fx_exp2(FX_LOG2_01 / (rel * fs));
    } break;
    case EC_FX_TAPE_ECHO: {
        ec_fxm_tape_t *t = &u->tp;
        t->fbg = fx_clip(p->delay_feedback, 0.f, 0.95f);
        t->wow = fx_clip(p->tape_wow, 0.f, 1.f);
        t->flut = fx_clip(p->tape_flutter, 0.f, 1.f);
        lpf_coef(&t->lp, fx_linexp(fx_clip(p->echo_tone, 0.f, 1.f), 0.f, 1.f, 300.f, 14000.f), fs);
        float sat = fx_clip(p->tape_sat, 0.f, 1.f);
        t->k1 = 1.f + sat * 5.f;
        t->k2 = 0.9f / (1.f + sat * 1.2f);
        fx_xfade(sat, &t->ga, &t->gb);
    } break;
    case EC_FX_CASSETTE: {
        ec_fxm_cass_t *c = &u->cs;
        c->wow = fx_clip(p->tape_wow, 0.f, 1.f);
        c->flut = fx_clip(p->tape_flutter, 0.f, 1.f);
        c->drop = fx_clip(p->cass_drop, 0.f, 1.f);
        c->noise = fx_clip(p->cass_noise, 0.f, 1.f) * 0.02f;
        c->dens = fx_clip(p->cass_crackle, 0.f, 1.f) * 40.f / fs;   /* Dust2 threshold */
        lpf_coef(&c->lp, fx_linexp(fx_clip(p->cass_tone, 0.f, 1.f), 0.f, 1.f, 1200.f, 16000.f), fs);
    } break;
    case EC_FX_MOTION: {
        ec_fxm_motion_t *m = &u->mo;
        m->sg = fx_clip(1.f + fx_clip(p->motion_width, -1.f, 1.f), 0.f, 2.f);
        m->trem = fx_clip(p->motion_trem, 0.f, 1.f);
        m->pan = fx_clip(p->motion_pan, 0.f, 1.f);
        m->shape = ec_clampi(p->motion_shape, 0, 2);
    } break;
    case EC_FX_RINGS: {
        ec_fxm_rings_t *r = &u->rg;
        float mag = fx_clip(p->rings_freq, -1.f, 1.f);
        float hz = mag == 0.f ? 0.f : fx_linexp(fabsf(mag), 0.001f, 1.f, 1.f, 2000.f) * (mag > 0.f ? 1.f : -1.f);
        float f = hz + fx_clip(p->rings_fine, -1.f, 1.f) * 20.f;
        r->mode = p->rings_mode >= 1 ? 1 : 0;
        if (!r->mode) { f = fabsf(f); if (f < 0.1f) f = 0.1f; }
        float w = f / fs;
        r->sr = fx_sin2pi(w);
        r->cr = fx_sin2pi(w + 0.25f);
        r->fbamt = fx_clip(p->rings_feedback, 0.f, 1.f) * 0.85f;
        lpf_coef(&r->lp, fx_linexp(fx_clip(p->modfx_tone, 0.f, 1.f), 0.f, 1.f, 400.f, 18000.f), fs);
    } break;
    case EC_FX_LIMIT: {
        ec_fxm_limit_t *l = &u->lm;
        l->g = fx_dbamp(fx_linlin(p->limit_gain, 0.f, 1.f, 0.f, 18.f));
        l->ceil = fx_dbamp(fx_linlin(p->limit_ceil, 0.f, 1.f, -12.f, 0.f));
        l->mc = fx_exp2(FX_LOG2_01 / (0.01f * fs));
        int32_t s = limit_size(fx);
        if (l->lim[0].size && s != l->lim[0].size) fx->restart_req = 1;  /* SC: dur is init-rate */
    } break;
    default: break;
    }
}

/* ------------------------------------------------------------ (re)start */

static void m_start(ec_fx_t *fx)
{
    ec_fxm_t *u = &fx->u;
    float *mem = fx->mem;
    uint32_t off = 0;
    uint32_t *rng = &fx->rng;
    memset(u, 0, sizeof *u);
    fx->prime = 0; fx->quiet = 0; fx->asleep = 0; fx->restart_req = 0;
    switch (fx->running) {
    case EC_FX_DELAY:
        line_carve(&u->dl.ln[0], mem, &off, fx->big_len);
        line_carve(&u->dl.ln[1], mem, &off, fx->big_len);
        break;
    case EC_FX_REVERB:
        for (int i = 0; i < 24; ++i) {
            u->rv.v[i].buf = mem + off; u->rv.v[i].len = FV_LEN[i];
            off += (uint32_t)FV_LEN[i];
        }
        break;
    case EC_FX_LOFI:
        u->lf.ph = 1.f;                     /* Impulse fires on the first sample */
        break;
    case EC_FX_COMP:
        u->cp.gain = 1.f;
        break;
    case EC_FX_DESTROY:
        line_carve(&u->ds.ln[0], mem, &off, fx->mod_len);
        line_carve(&u->ds.ln[1], mem, &off, fx->mod_len);
        n2_init(&u->ds.n2[0], rng); n2_init(&u->ds.n2[1], rng);
        u->ds.ph = 1.f;
        break;
    case EC_FX_ECHO:
        line_carve(&u->ec.ln[0], mem, &off, fx->big_len);
        line_carve(&u->ec.ln[1], mem, &off, fx->big_len);
        n2_init(&u->ec.n2[0], rng); n2_init(&u->ec.n2[1], rng);
        break;
    case EC_FX_BLACKHOLE: {
        int32_t ls = (int32_t)ceilf(0.01f * fx->fs);
        line_carve(&u->bh.pre[0], mem, &off, fx->pre_len);
        line_carve(&u->bh.pre[1], mem, &off, fx->pre_len);
        for (int i = 0; i < 8; ++i) { line_carve(&u->bh.ap[i], mem, &off, fx->ap_len); n2_init(&u->bh.n2[i], rng); }
        lim_carve(&u->bh.lim[0], mem, &off, ls);
        lim_carve(&u->bh.lim[1], mem, &off, ls);
        fx->prime = 2 * ls;
    } break;
    case EC_FX_CHORUS:
    case EC_FX_FLANGER:
        line_carve(&u->md.ln[0], mem, &off, fx->mod_len);
        line_carve(&u->md.ln[1], mem, &off, fx->mod_len);
        break;
    case EC_FX_DUCK:
        u->dk.gain = 1.f;
        break;
    case EC_FX_TAPE_ECHO:
        line_carve(&u->tp.ln[0], mem, &off, fx->big_len);
        line_carve(&u->tp.ln[1], mem, &off, fx->big_len);
        n2_init(&u->tp.n2, rng);
        hpf_coef(&u->tp.hp, 70.f, fx->fs);
        break;
    case EC_FX_CASSETTE:
        line_carve(&u->cs.ln[0], mem, &off, fx->mod_len);
        line_carve(&u->cs.ln[1], mem, &off, fx->mod_len);
        n2_init(&u->cs.n2, rng);
        u->cs.n1.a = ec_frand2(rng); u->cs.n1.b = ec_frand2(rng);
        hpf_coef(&u->cs.hp60, 60.f, fx->fs);
        hpf_coef(&u->cs.hp800, 800.f, fx->fs);
        u->cs.bp = fx->bp2500;
        pink_init(&u->cs.pink[0], rng); pink_init(&u->cs.pink[1], rng);
        break;
    case EC_FX_RINGS:
        u->rg.oc = 1.f; u->rg.os = 0.f;
        break;
    case EC_FX_LIMIT: {
        int32_t s = limit_size(fx);
        lim_carve(&u->lm.lim[0], mem, &off, s);
        lim_carve(&u->lm.lim[1], mem, &off, s);
        fx->prime = 2 * s;
    } break;
    default: break;
    }
    derive(fx);
    fx->gd = fx->gd_t; fx->gw = fx->gw_t;
    /* hold the fade-in until a fresh line's first output has arrived (the
     * delayed signal would otherwise step in part-way through the fade) */
    switch (fx->running) {
    case EC_FX_DESTROY:   fx->prime = (int32_t)(0.011f * fx->fs) + 4; break;
    case EC_FX_CHORUS:    fx->prime = (int32_t)((0.015f + u->md.depth) * fx->fs) + 4; break;
    case EC_FX_FLANGER:   fx->prime = (int32_t)((0.0006f + u->md.depth) * fx->fs) + 4; break;
    case EC_FX_CASSETTE:  fx->prime = (int32_t)((0.012f + 0.004f * u->cs.wow + 0.002f * u->cs.flut) * fx->fs) + 4; break;
    case EC_FX_BLACKHOLE: fx->prime += u->bh.pres + 4; break;
    default: break;
    }
}

/* ------------------------------------------------------------ machines */
/* Each: in (sanitized dry) -> wet. n <= EC_FX_KR. */

static void m_delay(ec_fx_t *fx, float bpm, const float *in[2], float *wet[2], int n)
{
    ec_fxm_delay_t *d = &fx->u.dl;
    float tgt = fx_clip((fmaxf(fx->pc.delay_beats, 0.03125f) * 60.f) / bpm, 0.001f, EC_FX_MAX_DELAY_S);
    float maxd = line_maxd(&d->ln[0]);
    if (!d->init) { d->t = tgt; d->ds = fx_clip(tgt * fx->fs, 1.f, maxd); d->init = 1; }
    else d->t = tgt + fx->lag15[n] * (d->t - tgt);
    float d1 = fx_clip(d->t * fx->fs, 1.f, maxd), inc = (d1 - d->ds) / (float)n;
    for (int ch = 0; ch < 2; ++ch) {
        ec_fxline_t *ln = &d->ln[ch];
        ec_fxbqs_t *st = &d->lps[ch];
        float fb = d->fb[ch], ds = d->ds;
        const float *x = in[ch];
        float *y = wet[ch];
        for (int i = 0; i < n; ++i) {
            ds += inc;
            line_write(ln, fz(x[i] + fb * d->fbg));
            float v = sc_softclip(lpf_tick(&d->lp, st, line_cubic(ln, ds)));
            fb = v;
            y[i] = v;
        }
        d->fb[ch] = fz(fb);
        bq_flush(st);
    }
    d->ds = d1;
}

/* FreeVerb2 channel tick (Faust code in SC ReverbUGens.cpp) */
static inline float fv_tick(ec_fxm_reverb_t *r, ec_fxfv_t *v, float in, int cold, int32_t k)
{
    float sum = 0.f;
    float T[4];
    for (int a = 0; a < 4; ++a) {               /* allpass reads first (SC order) */
        ec_fxfv_t *q = &v[a];
        if (++q->idx == q->len) q->idx = 0;
        T[a] = (cold && k < q->len) ? 0.f : q->buf[q->idx];
    }
    for (int c = 4; c < 12; ++c) {
        ec_fxfv_t *q = &v[c];
        if (++q->idx == q->len) q->idx = 0;
        float t = (cold && k < q->len) ? 0.f : q->buf[q->idx];
        q->f = r->d2 * q->r + r->d1 * q->f;
        q->buf[q->idx] = in + r->fb * q->f;
        q->r = t;
        sum += t;
    }
    float u = sum;
    for (int a = 3; a >= 0; --a) {
        ec_fxfv_t *q = &v[a];
        q->buf[q->idx] = 0.5f * q->r + u;
        q->r = T[a];
        u = q->r - u;
    }
    return u;
}

static void m_reverb(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_reverb_t *r = &fx->u.rv;
    if (r->n >= 1640) {
        for (int i = 0; i < n; ++i) {
            float x = 0.015f * (in[0][i] + in[1][i]);
            wet[0][i] = fv_tick(r, &r->v[0], x, 0, 0);
            wet[1][i] = fv_tick(r, &r->v[12], x, 0, 0);
        }
    } else {
        for (int i = 0; i < n; ++i) {
            float x = 0.015f * (in[0][i] + in[1][i]);
            wet[0][i] = fv_tick(r, &r->v[0], x, 1, r->n);
            wet[1][i] = fv_tick(r, &r->v[12], x, 1, r->n);
            r->n++;
        }
    }
    for (int i = 0; i < 24; ++i) { FL(r->v[i].r); FL(r->v[i].f); }
}

static void m_lofi(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_lofi_t *f = &fx->u.lf;
    float ph = f->ph, trig = f->trig;
    for (int i = 0; i < n; ++i) {
        float z = 0.f;
        if (ph >= 1.f) { ph -= 1.f; if (ph >= 1.f) ph = 1.f; z = 1.f; }
        ph += f->inc;
        if (z > 0.f && trig <= 0.f) { f->held[0] = in[0][i]; f->held[1] = in[1][i]; }
        trig = z;
        wet[0][i] = floorf(f->held[0] * f->inv + 0.5f) * f->step;
        wet[1][i] = floorf(f->held[1] * f->inv + 0.5f) * f->step;
    }
    f->ph = ph; f->trig = trig;
}

static void m_comp(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_comp_t *c = &fx->u.cp;
    float pm = c->prevmax, ai = c->amp_in, ao = c->amp_out;
    for (int i = 0; i < n; ++i) {
        float v = fabsf(0.5f * (in[0][i] + in[1][i]));
        pm = follow(pm, v, c->cc, c->rc);
        ai = follow(ai, v, c->mc, c->mc);
    }
    float next = 1.f;
    if (pm >= c->thresh && c->slope != 1.f) next = fx_exp2((c->slope - 1.f) * fx_log2(pm / c->thresh));
    float g = c->gain, gi = (next - g) / (float)n;
    for (int i = 0; i < n; ++i) {
        g += gi;
        float l = in[0][i] * g, r = in[1][i] * g;
        ao = follow(ao, fabsf(l), c->mc, c->mc);
        wet[0][i] = l * c->makeup;
        wet[1][i] = r * c->makeup;
    }
    c->gain = next;
    FL(pm); FL(ai); FL(ao);
    c->prevmax = pm; c->amp_in = ai; c->amp_out = ao;
    float gr = ao / (ai + 0.0001f);
    fx->meter.gr_db = fx_ampdb(fx_clip(gr, 0.0001f, 1.f));
    fx->meter.key_db = fx_ampdb(fx_clip(ai, 0.0001f, 4.f));
    fx->meter.thresh_db = fx_ampdb(c->thresh);
    fx->meter.valid = 1;
}

static void m_destroy(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_destroy_t *d = &fx->u.ds;
    float d1[2];
    float inc = d->nrate * (float)n / fx->fs;
    for (int ch = 0; ch < 2; ++ch) {
        float nz = n2_tick(&d->n2[ch], inc, &fx->rng);
        d1[ch] = fx_clip(0.006f + nz * d->warb * 0.005f, 0.0003f, 0.03f) * fx->fs;
        if (!d->init) d->ds[ch] = d1[ch];
    }
    d->init = 1;
    /* shared Impulse for the stereo Latch */
    float ph = d->ph, trig = d->trig;
    float tr[EC_FX_KR];
    for (int i = 0; i < n; ++i) {
        float z = 0.f;
        if (ph >= 1.f) { ph -= 1.f; if (ph >= 1.f) ph = 1.f; z = 1.f; }
        ph += d->sinc;
        tr[i] = (z > 0.f && trig <= 0.f) ? 1.f : 0.f;
        trig = z;
    }
    d->ph = ph; d->trig = trig;
    for (int ch = 0; ch < 2; ++ch) {
        ec_fxline_t *ln = &d->ln[ch];
        float ds = d->ds[ch], di = (d1[ch] - ds) / (float)n;
        float held = d->held[ch];
        for (int i = 0; i < n; ++i) {
            ds += di;
            line_write(ln, in[ch][i]);
            float x = line_cubic(ln, ds);
            if (d->driv > 0.f) x = d->dga * x + d->dgb * ec_tanh(x * d->dk + d->dofs);
            if (d->fgb > 0.f) x = d->fga * x + d->fgb * sc_fold1(x * d->fk);
            if (d->sgb > 0.f) x = d->sga * x + d->sgb * ec_tanh(x * d->sk);
            if (d->cgb > 0.f) x = d->cga * x + d->cgb * (floorf(x * d->cinv + 0.5f) * d->cstep);
            if (tr[i] > 0.f) held = x;
            if (d->sgb2 > 0.f) x = d->sga2 * x + d->sgb2 * held;
            x = lpf_tick(&d->lp, &d->lps[ch], x);
            wet[ch][i] = sc_softclip(x * d->lvl);
        }
        d->ds[ch] = d1[ch];
        d->held[ch] = held;
        bq_flush(&d->lps[ch]);
    }
}

static void m_echo(ec_fx_t *fx, float bpm, const float *in[2], float *wet[2], int n)
{
    ec_fxm_echo_t *e = &fx->u.ec;
    const float fs = fx->fs, maxd = line_maxd(&e->ln[0]);
    float tgt = fx_clip((fmaxf(fx->pc.echo_beats, 0.03125f) * 60.f) / bpm, 0.001f, EC_FX_MAX_DELAY_S);
    if (!e->init) { e->lt = tgt; e->off = e->offt; }
    else { float b = fx->lag15[n]; e->lt = tgt + b * (e->lt - tgt); e->off = e->offt + b * (e->off - e->offt); }
    float rt = fx_clip(e->lt * e->off, 0.001f, EC_FX_MAX_DELAY_S);
    float dt = (float)n / fs;
    e->ph[0] += 0.7f * dt; if (e->ph[0] >= 1.f) e->ph[0] -= 1.f;
    e->ph[1] += 0.73f * dt; if (e->ph[1] >= 1.f) e->ph[1] -= 1.f;
    float nl = n2_tick(&e->n2[0], 9.f * dt, &fx->rng), nr = n2_tick(&e->n2[1], 9.3f * dt, &fx->rng);
    float w = e->wob;
    float lm = fx_clip(e->lt * (1.f + fx_sin2pi(e->ph[0]) * w * 0.03f + nl * w * 0.01f), 0.001f, EC_FX_MAX_DELAY_S);
    float rm = fx_clip(rt * (1.f + fx_sin2pi(e->ph[1]) * w * 0.03f + nr * w * 0.01f), 0.001f, EC_FX_MAX_DELAY_S);
    float d1[2] = { fx_clip(lm * fs, 1.f, maxd), fx_clip(rm * fs, 1.f, maxd) };
    if (!e->init) { e->ds[0] = d1[0]; e->ds[1] = d1[1]; e->init = 1; }
    float di0 = (d1[0] - e->ds[0]) / (float)n, di1 = (d1[1] - e->ds[1]) / (float)n;
    float ds0 = e->ds[0], ds1 = e->ds[1];
    float f0 = e->fb[0], f1 = e->fb[1];
    for (int i = 0; i < n; ++i) {
        ds0 += di0; ds1 += di1;
        float fl = e->mode ? f1 : f0, fr = e->mode ? f0 : f1;
        line_write(&e->ln[0], fz(in[0][i] + fl * e->fbg));
        line_write(&e->ln[1], fz(in[1][i] + fr * e->fbg));
        f0 = sc_softclip(lpf_tick(&e->lp, &e->lps[0], line_cubic(&e->ln[0], ds0)));
        f1 = sc_softclip(lpf_tick(&e->lp, &e->lps[1], line_cubic(&e->ln[1], ds1)));
        wet[0][i] = f0; wet[1][i] = f1;
    }
    e->fb[0] = fz(f0); e->fb[1] = fz(f1);
    e->ds[0] = d1[0]; e->ds[1] = d1[1];
    bq_flush(&e->lps[0]); bq_flush(&e->lps[1]);
}

static void m_blackhole(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_bh_t *b = &fx->u.bh;
    const float fs = fx->fs, dt = (float)n / fs, maxd = line_maxd(&b->ap[0]);
    float d1[8], g[8];
    for (int k = 0; k < 4; ++k) {
        for (int ch = 0; ch < 2; ++ch) {
            int j = k * 2 + ch;
            float nz = n2_tick(&b->n2[j], (0.2f + (float)k * 0.13f) * dt, &fx->rng);
            float mt = fx_clip(BH_T[k] * b->size * (1.f + nz * b->mod * 0.18f), 0.001f, 0.12f);
            d1[j] = fx_clip(mt * fs - 1.f, 1.f, maxd);      /* read-before-write: age = dsamp - 1 */
            g[j] = fx_exp2(FX_LOG2_001 * mt / b->decay);
            if (!b->init) b->ds[j] = d1[j];
        }
    }
    b->init = 1;
    for (int ch = 0; ch < 2; ++ch) {
        float ds[4], di[4];
        for (int k = 0; k < 4; ++k) { ds[k] = b->ds[k * 2 + ch]; di[k] = (d1[k * 2 + ch] - ds[k]) / (float)n; }
        float loop = b->loop[ch], dcx = b->dcx[ch], dcy = b->dcy[ch];
        ec_fxlim_t *lim = &b->lim[ch];
        for (int i = 0; i < n; ++i) {
            line_write(&b->pre[ch], in[ch][i]);
            float v = line_age(&b->pre[ch], b->pres) + loop * b->fbg;
            for (int k = 0; k < 4; ++k) {
                ec_fxline_t *ln = &b->ap[k * 2 + ch];
                float gk = g[k * 2 + ch];
                ds[k] += di[k];
                float val = line_cubic(ln, ds[k]);
                float dwr = fz(v + gk * val);
                line_write(ln, dwr);
                v = val - gk * dwr;
            }
            v = lpf_tick(&b->lp, &b->lps[ch], v);
            v = hpf_tick(&b->hp, &b->hps[ch], v);
            float y = v - dcx + 0.995f * dcy;
            dcx = v; dcy = y;
            loop = lim_tick(lim, y, 0.9f);
            wet[ch][i] = loop;
        }
        b->loop[ch] = fz(loop); b->dcx[ch] = fz(dcx); b->dcy[ch] = fz(dcy);
        bq_flush(&b->lps[ch]); bq_flush(&b->hps[ch]);
    }
    for (int j = 0; j < 8; ++j) b->ds[j] = d1[j];
}

static void m_chorus(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_mod_t *m = &fx->u.md;
    const float fs = fx->fs, maxd = line_maxd(&m->ln[0]);
    m->ph += m->rate * (float)n / fs;
    if (m->ph >= 1.f) m->ph -= 1.f;
    float s[2] = { fx_sin2pi(m->ph), fx_sin2pi(m->ph + m->wofs) };
    for (int ch = 0; ch < 2; ++ch) {
        float d1 = fx_clip(fx_clip(0.015f + m->depth * s[ch], 0.001f, 0.05f) * fs, 1.f, maxd);
        if (!m->init) m->ds[ch] = d1;
        float ds = m->ds[ch], di = (d1 - ds) / (float)n;
        for (int i = 0; i < n; ++i) {
            ds += di;
            line_write(&m->ln[ch], in[ch][i]);
            wet[ch][i] = lpf_tick(&m->lp, &m->lps[ch], line_cubic(&m->ln[ch], ds));
        }
        m->ds[ch] = d1;
        bq_flush(&m->lps[ch]);
    }
    m->init = 1;
}

static void m_flanger(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_mod_t *m = &fx->u.md;
    const float fs = fx->fs;
    m->ph += m->rate * (float)n / fs;
    if (m->ph >= 1.f) m->ph -= 1.f;
    float lfo = 0.5f * (fx_sin2pi(m->ph) + 1.f);
    float d1 = fx_clip(fx_clip(0.0006f + m->depth * lfo, 0.0002f, 0.02f) * fs, 1.f, line_maxd(&m->ln[0]));
    if (!m->init) { m->ds[0] = d1; m->init = 1; }
    float di = (d1 - m->ds[0]) / (float)n;
    for (int ch = 0; ch < 2; ++ch) {
        float ds = m->ds[0], fb = m->fb[ch];
        for (int i = 0; i < n; ++i) {
            ds += di;
            line_write(&m->ln[ch], fz(in[ch][i] + fb * m->fbamt));
            fb = lpf_tick(&m->lp, &m->lps[ch], line_cubic(&m->ln[ch], ds));
            wet[ch][i] = fb;
        }
        m->fb[ch] = fz(fb);
        bq_flush(&m->lps[ch]);
    }
    m->ds[0] = d1;
}

static void m_phaser(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_phaser_t *h = &fx->u.ph;
    const float fs = fx->fs;
    h->ph += h->rate * (float)n / fs;
    if (h->ph >= 1.f) h->ph -= 1.f;
    float fc = h->lo + (h->hi - h->lo) * 0.5f * (fx_sin2pi(h->ph) + 1.f);
    fc = fx_clip(fc, 40.f, fx_clip(12000.f, 40.f, 0.45f * fs));
    float t = fx_tan(FX_PI * fc / fs);
    float c = (t - 1.f) / (t + 1.f);
    int ns = h->sel + 1;
    if (ns > h->nst) {                       /* stages that sat idle start clean */
        for (int ch = 0; ch < 2; ++ch)
            for (int k = h->nst; k < ns; ++k) { h->x1[ch][k] = 0.f; h->y1[ch][k] = 0.f; }
    }
    h->nst = ns;
    for (int ch = 0; ch < 2; ++ch) {
        float *x1 = h->x1[ch], *y1 = h->y1[ch];
        float fb = h->fb[ch];
        for (int i = 0; i < n; ++i) {
            float x = in[ch][i] + fb * h->fbamt;
            for (int k = 0; k < ns; ++k) {
                float y = c * x + x1[k] - c * y1[k];
                x1[k] = x; y1[k] = y;
                x = y;
            }
            fb = x;
            wet[ch][i] = (in[ch][i] + x) * 0.5f;
        }
        h->fb[ch] = fz(fb);
        for (int k = 0; k < ns; ++k) { FL(x1[k]); FL(y1[k]); }
    }
    h->c = c;
}

static void m_djeq(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_eq_t *q = &fx->u.eq;
    float dl = q->gl - q->gm, dh = q->gh - q->gm;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < n; ++i) {
            float x = in[ch][i];
            float lo = lpf_tick(&q->lp, &q->lps[ch], x);
            float hi = hpf_tick(&q->hp, &q->hps[ch], x);
            /* low*gL + (x - low - high)*gM + high*gH, exact x at unity */
            wet[ch][i] = x * q->gm + lo * dl + hi * dh;
        }
        bq_flush(&q->lps[ch]); bq_flush(&q->hps[ch]);
    }
}

static void m_duck(ec_fx_t *fx, const float *kl, const float *kr, const float *in[2], float *wet[2], int n)
{
    ec_fxm_duck_t *k = &fx->u.dk;
    float amp = k->amp, g = k->gain;
    const float b = fx->duck_lag;
    for (int i = 0; i < n; ++i) {
        float a = kl ? kl[i] : 0.f, c = kr ? kr[i] : 0.f;
        float km = (a + c) * 0.5f;
        float v = fabsf(km);
        if (!(v < 1e4f)) v = 0.f;            /* sanitize the key */
        if (!k->init) { amp = v; }
        amp = follow(amp, v, k->cc, k->rc);
        float env = amp * k->sens;
        if (env > 1.f) env = 1.f;
        float tgt = fx_clip(1.f - env * k->amt, 0.f, 1.f);
        if (!k->init) { g = tgt; k->init = 1; }
        g = tgt + b * (g - tgt);
        wet[0][i] = in[0][i] * g;
        wet[1][i] = in[1][i] * g;
    }
    FL(amp);
    k->amp = amp; k->gain = g;
    fx->meter.gr_db = fx_ampdb(fx_clip(g, 0.0001f, 1.f));
    fx->meter.key_db = fx_ampdb(fx_clip(amp, 0.0001f, 4.f));
    fx->meter.thresh_db = -99.f;
    fx->meter.valid = 1;
}

static void m_tape(ec_fx_t *fx, float bpm, const float *in[2], float *wet[2], int n)
{
    ec_fxm_tape_t *t = &fx->u.tp;
    const float fs = fx->fs, dt = (float)n / fs, maxd = line_maxd(&t->ln[0]);
    float tgt = fx_clip((fmaxf(fx->pc.echo_beats, 0.03125f) * 60.f) / bpm, 0.001f, EC_FX_MAX_DELAY_S);
    if (!t->init) t->t = tgt; else t->t = tgt + fx->lag15[n] * (t->t - tgt);
    t->ph += 0.5f * dt; if (t->ph >= 1.f) t->ph -= 1.f;
    float wow = fx_sin2pi(t->ph) * t->wow * 0.02f;
    float flu = n2_tick(&t->n2, 8.f * dt, &fx->rng) * t->flut * 0.006f;
    float d1 = fx_clip(fx_clip(t->t * (1.f + wow + flu), 0.001f, EC_FX_MAX_DELAY_S) * fs, 1.f, maxd);
    if (!t->init) { t->ds = d1; t->init = 1; }
    float di = (d1 - t->ds) / (float)n;
    for (int ch = 0; ch < 2; ++ch) {
        float ds = t->ds, fb = t->fb[ch];
        for (int i = 0; i < n; ++i) {
            ds += di;
            line_write(&t->ln[ch], fz(in[ch][i] + fb * t->fbg));
            float r = line_cubic(&t->ln[ch], ds);
            r = lpf_tick(&t->lp, &t->lps[ch], hpf_tick(&t->hp, &t->hps[ch], r));
            if (t->gb > 0.f) r = t->ga * r + t->gb * (ec_tanh(r * t->k1) * t->k2);
            fb = sc_softclip(r);
            wet[ch][i] = fb;
        }
        t->fb[ch] = fz(fb);
        bq_flush(&t->lps[ch]); bq_flush(&t->hps[ch]);
    }
    t->ds = d1;
}

static void m_cassette(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_cass_t *c = &fx->u.cs;
    const float fs = fx->fs, dt = (float)n / fs;
    c->ph += 0.6f * dt;
    if (c->ph >= 1.f) c->ph -= 1.f;
    float nz = n2_tick(&c->n2, 6.f * dt, &fx->rng);
    float tw = fx_clip(0.012f + fx_sin2pi(c->ph) * c->wow * 0.004f + nz * c->flut * 0.002f, 0.001f, 0.03f);
    float d1 = fx_clip(tw * fs, 1.f, line_maxd(&c->ln[0]));
    float u = 0.5f * (n1_tick(&c->n1, 3.f * dt, &fx->rng) + 1.f);
    float dm1 = fx_clip(1.f - c->drop * u * u * u, 0.f, 1.f);
    if (!c->init) { c->ds = d1; c->dm = dm1; c->init = 1; }
    float di = (d1 - c->ds) / (float)n, dmi = (dm1 - c->dm) / (float)n;
    int hiss = c->noise > 0.f || c->h800s[0].y1 != 0.f || c->h800s[1].y1 != 0.f;
    int crack = c->dens > 0.f || c->dec[0] != 0.f || c->dec[1] != 0.f ||
                c->bps[0].y1 != 0.f || c->bps[1].y1 != 0.f;
    float scale = c->dens > 0.f ? 2.f / c->dens : 0.f;
    for (int ch = 0; ch < 2; ++ch) {
        float ds = c->ds, dm = c->dm, dec = c->dec[ch];
        for (int i = 0; i < n; ++i) {
            ds += di; dm += dmi;
            line_write(&c->ln[ch], in[ch][i]);
            float w = line_cubic(&c->ln[ch], ds) * dm;
            float y = lpf_tick(&c->lp, &c->lps[ch], hpf_tick(&c->hp60, &c->h60s[ch], w));
            if (hiss) y += hpf_tick(&c->hp800, &c->h800s[ch], pink_tick(&c->pink[ch], &fx->rng) * c->noise);
            if (crack) {
                /* Dust2 -> Decay(2 ms) -> BPF(2500, rq 1.5) * 0.4 */
                float z = (float)(ec_xorshift(&fx->rng) & 0xFFFFFFu) * (1.f / 16777216.f);
                dec = (z < c->dens ? z * scale - 1.f : 0.f) + fx->dec002 * dec;
                y += bpf_tick(&c->bp, &c->bps[ch], dec) * 0.4f;
            }
            wet[ch][i] = y;
        }
        c->dec[ch] = fz(dec);
        bq_flush(&c->lps[ch]); bq_flush(&c->h60s[ch]); bq_flush(&c->h800s[ch]); bq_flush(&c->bps[ch]);
    }
    c->ds = d1; c->dm = dm1;
}

static void m_motion(ec_fx_t *fx, float bpm, const float *in[2], float *wet[2], int n)
{
    ec_fxm_motion_t *m = &fx->u.mo;
    float hz = fx_clip((bpm / 60.f) / fmaxf(fx->pc.motion_beats, 0.0625f), 0.01f, 40.f);
    m->ph += hz * (float)n / fx->fs;
    while (m->ph >= 1.f) m->ph -= 1.f;
    float p = m->ph, u1;
    if (m->shape == 0) u1 = 0.5f * (fx_sin2pi(p) + 1.f);
    else if (m->shape == 1) u1 = 0.5f * ((p < 0.25f ? 4.f * p : (p < 0.75f ? 2.f - 4.f * p : 4.f * p - 4.f)) + 1.f);
    else u1 = p < 0.5f ? 1.f : 0.f;
    if (!m->init) { m->u = u1; m->init = 1; }
    float u = m->u, ui = (u1 - u) / (float)n;
    m->u = u1;
    if (m->trem == 0.f && m->pan == 0.f && m->sg == 1.f) {
        memcpy(wet[0], in[0], (size_t)n * sizeof(float));
        memcpy(wet[1], in[1], (size_t)n * sizeof(float));
        return;
    }
    for (int i = 0; i < n; ++i) {
        u += ui;
        float l = in[0][i], r = in[1][i];
        if (m->sg != 1.f) {
            float mid = (l + r) * 0.5f, side = (l - r) * 0.5f * m->sg;
            l = mid + side; r = mid - side;
        }
        float tr = 1.f - m->trem * u;
        wet[0][i] = l * tr * (1.f - m->pan * u);
        wet[1][i] = r * tr * (1.f - m->pan * (1.f - u));
    }
}

static void m_rings(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_rings_t *r = &fx->u.rg;
    const float *hc = fx->hcoef;
    float s = r->os, c = r->oc;
    for (int i = 0; i < n; ++i) {
        for (int ch = 0; ch < 2; ++ch) {
            float x = in[ch][i] + r->fb[ch] * r->fbamt, y;
            if (!r->mode) y = x * s;
            else {
                float *hy = r->hy[ch];
                float a = x, b = x;
                for (int k = 0; k < 6; ++k) {
                    float y0 = a - hc[k] * hy[k];
                    a = hc[k] * y0 + hy[k];
                    hy[k] = y0;
                }
                for (int k = 6; k < 12; ++k) {
                    float y0 = b - hc[k] * hy[k];
                    b = hc[k] * y0 + hy[k];
                    hy[k] = y0;
                }
                y = a * c + b * s;           /* FreqShift: outcos*cos + outsin*sin */
            }
            y = lpf_tick(&r->lp, &r->lps[ch], y);
            r->fb[ch] = sc_softclip(y);
            wet[ch][i] = y;
        }
        float ns = s * r->cr + c * r->sr;
        c = c * r->cr - s * r->sr;
        s = ns;
    }
    float k = 1.5f - 0.5f * (s * s + c * c);
    r->os = s * k; r->oc = c * k;
    for (int ch = 0; ch < 2; ++ch) {
        r->fb[ch] = fz(r->fb[ch]);
        for (int j = 0; j < 12; ++j) FL(r->hy[ch][j]);
        bq_flush(&r->lps[ch]);
    }
}

static void m_limit(ec_fx_t *fx, const float *in[2], float *wet[2], int n)
{
    ec_fxm_limit_t *l = &fx->u.lm;
    float ai = l->amp_in, ao = l->amp_out;
    for (int i = 0; i < n; ++i) {
        float a = in[0][i] * l->g, b = in[1][i] * l->g;
        float ya = lim_tick(&l->lim[0], a, l->ceil);
        float yb = lim_tick(&l->lim[1], b, l->ceil);
        ai = follow(ai, fabsf(a), l->mc, l->mc);
        ao = follow(ao, fabsf(ya), l->mc, l->mc);
        wet[0][i] = ya; wet[1][i] = yb;
    }
    FL(ai); FL(ao);
    l->amp_in = ai; l->amp_out = ao;
    fx->meter.gr_db = fx_ampdb(fx_clip(ao / (ai + 0.0001f), 0.0001f, 1.f));
    fx->meter.key_db = fx_ampdb(fx_clip(ai, 0.0001f, 4.f));
    fx->meter.thresh_db = fx_ampdb(l->ceil);
    fx->meter.valid = 1;
}

/* ------------------------------------------------------------ slot */

void ec_fx_init(ec_fx_t *fx, float *mem, float fs, int is_send, uint32_t seed)
{
    memset(fx, 0, sizeof *fx);
    if (!(fs >= 8000.f && fs <= 192000.f)) fs = 48000.f;
    fx->mem = mem;
    fx->fs = fs;
    fx->is_send = is_send ? 1 : 0;
    fx->seed = seed;
    fx->rng = seed ? seed : 0x9E3779B9u;
    fx->running = fx->want = EC_FX_NONE;
    fx->g = 1.f;
    fx->gstep = 1.f / (0.01f * fs);
    int32_t big = (int32_t)ceilf(EC_FX_MAX_DELAY_S * fs) + 8;
    if (big > (int32_t)EC_FX_LINE_FRAMES - 3) big = (int32_t)EC_FX_LINE_FRAMES - 3;
    fx->big_len = big;
    fx->ap_len = (int32_t)(0.09f * fs) + 8;    /* max AllpassC time 0.043*1.6*1.18 s */
    fx->pre_len = (int32_t)(0.2f * fs) + 8;
    fx->mod_len = (int32_t)(0.05f * fs) + 8;   /* chorus 50 ms; flanger/destroy/cassette less */
    fx->lag15[0] = 1.f;
    for (int i = 1; i <= EC_FX_KR; ++i) fx->lag15[i] = expf(logf(0.001f) * (float)i / (0.15f * fs));
    {
        static const double G[12] = {0.3609, 2.7412, 11.1573, 44.7581, 179.6242, 798.4578,
                                      1.2524, 5.5671, 22.3423, 89.6271, 364.7914, 2770.1114};
        double gc = (15.0 * 3.14159265358979323846) / (double)fs;
        for (int i = 0; i < 12; ++i) { double g = gc * G[i]; fx->hcoef[i] = (float)((g - 1.0) / (g + 1.0)); }
    }
    {
        double pf = 2500.0 * 2.0 * 3.14159265358979323846 / (double)fs;
        double pbw = 1.5 * pf * 0.5;
        double C = 1.0 / tan(pbw), D = 2.0 * cos(pf);
        fx->bp2500.a0 = (float)(1.0 / (1.0 + C));
        fx->bp2500.b1 = (float)(C * D / (1.0 + C));
        fx->bp2500.b2 = (float)((1.0 - C) / (1.0 + C));
    }
    fx->dec002 = expf(logf(0.001f) / (0.002f * fs));
    fx->duck_lag = expf(logf(0.001f) / (0.005f * fs));
    ec_fx_params_default(&fx->pc);
    fx->gd = fx->gd_t = 1.f;
    fx->gw = fx->gw_t = 0.f;
}

static int32_t sleep_window(const ec_fx_t *fx)
{
    switch (fx->running) {
    case EC_FX_NONE: case EC_FX_COMP: case EC_FX_DUCK: case EC_FX_LIMIT:
        return -1;                                   /* never (meters / key / lookahead) */
    case EC_FX_DELAY: case EC_FX_ECHO: case EC_FX_TAPE_ECHO:
        return fx->big_len + 64;
    case EC_FX_BLACKHOLE:
        return (int32_t)(0.7f * fx->fs) + 2 * fx->u.bh.lim[0].size;
    case EC_FX_CHORUS: case EC_FX_FLANGER: case EC_FX_DESTROY: case EC_FX_CASSETTE:
        return fx->mod_len + 2048;
    case EC_FX_MOTION:
        return 64;
    default:
        return 4096;
    }
}

static void run_machine(ec_fx_t *fx, float bpm, const float *kl, const float *kr,
                        const float *in[2], float *wet[2], int n)
{
    switch (fx->running) {
    case EC_FX_DELAY:     m_delay(fx, bpm, in, wet, n); break;
    case EC_FX_REVERB:    m_reverb(fx, in, wet, n); break;
    case EC_FX_LOFI:      m_lofi(fx, in, wet, n); break;
    case EC_FX_COMP:      m_comp(fx, in, wet, n); break;
    case EC_FX_DESTROY:   m_destroy(fx, in, wet, n); break;
    case EC_FX_ECHO:      m_echo(fx, bpm, in, wet, n); break;
    case EC_FX_BLACKHOLE: m_blackhole(fx, in, wet, n); break;
    case EC_FX_CHORUS:    m_chorus(fx, in, wet, n); break;
    case EC_FX_FLANGER:   m_flanger(fx, in, wet, n); break;
    case EC_FX_PHASER:    m_phaser(fx, in, wet, n); break;
    case EC_FX_DJ_EQ:     m_djeq(fx, in, wet, n); break;
    case EC_FX_DUCK:      m_duck(fx, kl, kr, in, wet, n); break;
    case EC_FX_TAPE_ECHO: m_tape(fx, bpm, in, wet, n); break;
    case EC_FX_CASSETTE:  m_cassette(fx, in, wet, n); break;
    case EC_FX_MOTION:    m_motion(fx, bpm, in, wet, n); break;
    case EC_FX_RINGS:     m_rings(fx, in, wet, n); break;
    case EC_FX_LIMIT:     m_limit(fx, in, wet, n); break;
    default:
        if (fx->is_send) { memset(wet[0], 0, (size_t)n * sizeof(float)); memset(wet[1], 0, (size_t)n * sizeof(float)); }
        else { memcpy(wet[0], in[0], (size_t)n * sizeof(float)); memcpy(wet[1], in[1], (size_t)n * sizeof(float)); }
        break;
    }
}

static void switch_machine(ec_fx_t *fx)
{
    fx->running = fx->want;
    m_start(fx);
    if (fx->running == EC_FX_NONE) { fx->fdir = 0; fx->g = 1.f; }
    else { fx->fdir = 1; fx->g = 0.f; }
}

static void chunk(ec_fx_t *fx, float bpm, const float *kl, const float *kr, float *L, float *R, int n)
{
    if (fx->fdir == 0 && fx->running == EC_FX_NONE) {
        if (fx->is_send) { memset(L, 0, (size_t)n * sizeof(float)); memset(R, 0, (size_t)n * sizeof(float)); }
        return;
    }
    /* sanitize: NaN / Inf / > 1e4 -> 0, < 1e-30 -> 0 */
    float pk = 0.f;
    for (int i = 0; i < n; ++i) {
        float a = fabsf(L[i]), b = fabsf(R[i]);
        if (!(a < 1e4f) || a < 1e-30f) { L[i] = 0.f; a = 0.f; }
        if (!(b < 1e4f) || b < 1e-30f) { R[i] = 0.f; b = 0.f; }
        if (a > pk) pk = a;
        if (b > pk) pk = b;
    }
    if (fx->asleep) {
        if (fx->fdir == 0 && pk < FX_QUIET) {
            memset(L, 0, (size_t)n * sizeof(float)); memset(R, 0, (size_t)n * sizeof(float));
            return;
        }
        m_start(fx);                                 /* wake clean */
        fx->prime = 0;
    }

    float wl[EC_FX_KR], wr[EC_FX_KR];
    const float *in[2] = {L, R};
    float *wet[2] = {wl, wr};
    run_machine(fx, bpm, kl, kr, in, wet, n);

    const int mixm = FX_HAS_MIX[fx->running];
    float gd = fx->gd, gw = fx->gw;
    const float inv = 1.f / (float)n;
    const float gdi = (fx->gd_t - gd) * inv, gwi = (fx->gw_t - gw) * inv;
    fx->gd = fx->gd_t; fx->gw = fx->gw_t;

    if (fx->fdir == 0) {
        if (mixm) {
            for (int i = 0; i < n; ++i) {
                gd += gdi; gw += gwi;
                L[i] = gd * L[i] + gw * wl[i];
                R[i] = gd * R[i] + gw * wr[i];
            }
        } else {
            memcpy(L, wl, (size_t)n * sizeof(float));
            memcpy(R, wr, (size_t)n * sizeof(float));
        }
        int32_t win = sleep_window(fx);
        if (win >= 0) {
            float op = 0.f;
            if (pk < FX_QUIET && (!mixm || fx->gw_t > 1e-6f)) {
                for (int i = 0; i < n; ++i) { op = fmaxf(op, fabsf(L[i])); op = fmaxf(op, fabsf(R[i])); }
                if (op < FX_QUIET) {
                    fx->quiet += n;
                    if (fx->quiet > win) {
                        fx->asleep = 1; fx->quiet = 0;
                        memset(L, 0, (size_t)n * sizeof(float)); memset(R, 0, (size_t)n * sizeof(float));
                    }
                } else fx->quiet = 0;
            } else fx->quiet = 0;
        }
        return;
    }

    /* swap fade: machine output <-> dry (insert) / silence (send) */
    float g = fx->g;
    const float dk = fx->is_send ? 0.f : 1.f;
    float gs;
    if (fx->fdir < 0) gs = -fx->gstep;
    else if (fx->prime > 0) { gs = 0.f; fx->prime -= n; }
    else gs = fx->gstep;
    for (int i = 0; i < n; ++i) {
        float ol, orr;
        if (mixm) { gd += gdi; gw += gwi; ol = gd * L[i] + gw * wl[i]; orr = gd * R[i] + gw * wr[i]; }
        else { ol = wl[i]; orr = wr[i]; }
        float h = (1.f - g) * dk;
        L[i] = g * ol + h * L[i];
        R[i] = g * orr + h * R[i];
        g += gs;
        if (g < 0.f) g = 0.f;
        if (g > 1.f) g = 1.f;
    }
    fx->g = g;
    if (fx->fdir < 0 && g <= 0.f) switch_machine(fx);
    else if (fx->fdir > 0 && g >= 1.f) { fx->fdir = 0; fx->g = 1.f; }
}

void ec_fx_process(ec_fx_t *fx, const ec_fx_params_t *p, float bpm,
                   const float *keyL, const float *keyR, float *L, float *R, int n)
{
    if (!fx || !p || !L || !R || n <= 0) return;
    while (n > EC_FX_MAX_N) {
        ec_fx_process(fx, p, bpm, keyL, keyR, L, R, EC_FX_MAX_N);
        L += EC_FX_MAX_N; R += EC_FX_MAX_N;
        if (keyL) keyL += EC_FX_MAX_N;
        if (keyR) keyR += EC_FX_MAX_N;
        n -= EC_FX_MAX_N;
    }
    int want = p->machine;
    if (want < 1 || want > EC_FX_COUNT) want = EC_FX_NONE;
    fx->want = want;
    if (!(bpm >= 1.f)) bpm = 1.f;
    if (bpm > 2000.f) bpm = 2000.f;
    if (memcmp(p, &fx->pc, sizeof *p) != 0) { fx->pc = *p; derive(fx); }

    int swap = want != fx->running || fx->restart_req;
    if (swap) {
        if (fx->fdir == 0 && fx->running == EC_FX_NONE) switch_machine(fx);   /* nothing to fade out */
        else if (fx->fdir >= 0) fx->fdir = -1;
    } else if (fx->fdir < 0) {
        fx->fdir = 1;                                                          /* changed back */
    }
    fx->meter.valid = 0;
    for (int off = 0; off < n; off += EC_FX_KR) {
        int m = n - off < EC_FX_KR ? n - off : EC_FX_KR;
        chunk(fx, bpm, keyL ? keyL + off : NULL, keyR ? keyR + off : NULL, L + off, R + off, m);
    }
    int r = fx->running;
    if (r != EC_FX_COMP && r != EC_FX_DUCK && r != EC_FX_LIMIT) fx->meter.valid = 0;
}

void ec_fx_clear(ec_fx_t *fx)
{
    if (!fx) return;
    if (fx->running != EC_FX_NONE) m_start(fx);
    fx->prime = 0;
}

int ec_fx_running(const ec_fx_t *fx) { return fx->running; }

void ec_fx_meter(const ec_fx_t *fx, ec_fx_meter_t *m) { *m = fx->meter; }
