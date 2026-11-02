/*
 * test_fx.c -- E4 FX machines (src/dsp/ec_fx.c): bounds at extreme settings,
 * tails, click-free swaps, dynamics, unity paths, and a per-machine benchmark.
 *   make -C tests/dsp testfx
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../src/dsp/ec_fx.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define FS   44100
#define BLK  16
#define PI_D 3.14159265358979323846

static float g_mem[EC_FX_MEM_FLOATS] __attribute__((aligned(16)));
static ec_fx_t g_fx;

static uint32_t rs = 1u;
static float frand(void) { rs = rs * 1664525u + 1013904223u; return (float)(rs >> 8) / 16777216.f; }
static float frand2(void) { return 2.f * frand() - 1.f; }

static int is_sub(float x) { return fpclassify(x) == FP_SUBNORMAL; }

/* ------------------------------------------------------------ params */

typedef struct { size_t off; float lo, hi; } frange_t;
#define F(name, lo, hi) { offsetof(ec_fx_params_t, name), lo, hi }
static const frange_t FR[] = {
    F(mix, 0, 1), F(drive, 0, 1),
    F(delay_beats, 0.03125f, 8), F(delay_feedback, 0, 1), F(delay_tone, 0, 1),
    F(reverb_size, 0, 1), F(reverb_damp, 0, 1),
    F(lofi_bits, 1, 24), F(lofi_rate, 1000, 48000),
    F(comp_thresh, 0, 1), F(comp_ratio, 0, 1), F(comp_attack, 0, 1), F(comp_release, 0, 1), F(comp_makeup, 0, 1),
    F(destroy_warble, 0, 1), F(destroy_fold, 0, 1), F(destroy_sat, 0, 1), F(destroy_crush, 0, 1),
    F(destroy_srr, 0, 1), F(destroy_tone, 0, 1), F(destroy_level, 0, 1),
    F(echo_beats, 0.03125f, 8), F(echo_offset, -1, 1), F(echo_tone, 0, 1), F(echo_wobble, 0, 1),
    F(bh_gravity, 0, 1), F(bh_mod, 0, 1), F(bh_low, 0, 1), F(bh_predelay, 0, 1),
    F(modfx_rate, 0, 1), F(modfx_depth, 0, 1), F(modfx_tone, 0, 1), F(chorus_width, 0, 1),
    F(flanger_feedback, -1, 1), F(phaser_center, 0, 1), F(phaser_feedback, 0, 1),
    F(eq_low, -1, 1), F(eq_mid, -1, 1), F(eq_high, -1, 1), F(eq_xlow, 0, 1), F(eq_xhi, 0, 1),
    F(duck_amount, 0, 1), F(duck_sens, 0, 1), F(duck_attack, 0, 1), F(duck_release, 0, 1),
    F(tape_wow, 0, 1), F(tape_flutter, 0, 1), F(tape_sat, 0, 1),
    F(cass_noise, 0, 1), F(cass_crackle, 0, 1), F(cass_drop, 0, 1), F(cass_tone, 0, 1),
    F(motion_width, -1, 1), F(motion_beats, 0.0625f, 8), F(motion_trem, 0, 1), F(motion_pan, 0, 1),
    F(rings_freq, -1, 1), F(rings_fine, -1, 1), F(rings_feedback, 0, 1),
    F(limit_gain, 0, 1), F(limit_ceil, 0, 1), F(limit_release, 0, 1),
};
#define NFR ((int)(sizeof FR / sizeof FR[0]))

/* mode 0 = min, 1 = max, 2 = random */
static void set_params(ec_fx_params_t *p, int machine, int mode)
{
    ec_fx_params_default(p);
    p->machine = machine;
    for (int i = 0; i < NFR; ++i) {
        float v = mode == 0 ? FR[i].lo : mode == 1 ? FR[i].hi : FR[i].lo + (FR[i].hi - FR[i].lo) * frand();
        *(float *)((char *)p + FR[i].off) = v;
    }
    if (mode == 0) { p->echo_mode = 0; p->phaser_stages = 0; p->motion_shape = 0; p->rings_mode = 0; }
    else if (mode == 1) { p->echo_mode = 1; p->phaser_stages = 7; p->motion_shape = 2; p->rings_mode = 1; }
    else {
        p->echo_mode = (int)(frand() * 2); p->phaser_stages = (int)(frand() * 8);
        p->motion_shape = (int)(frand() * 3); p->rings_mode = (int)(frand() * 2);
    }
}

/* ------------------------------------------------------------ 1. extremes */

static float test_signal(int i, int ch)
{
    int seg = i / (FS * 5 / 2);
    switch (seg) {
    case 0: return frand2();
    case 1: return (i % 4410 == 0) ? ((i / 4410) & 1 ? -1.f : 1.f) : 0.f;
    case 2: return 1.f;
    default: return 0.99f * (float)sin(2 * PI_D * (ch ? 330.0 : 220.0) * i / FS);
    }
}

static float run_extreme(int machine, int is_send, int mode, float bpm, int *finite_ok)
{
    ec_fx_params_t p;
    set_params(&p, machine, mode);
    ec_fx_init(&g_fx, g_mem, FS, is_send, 1234u + (uint32_t)machine);
    float L[BLK], R[BLK], KL[BLK], KR[BLK], mx = 0.f;
    int ok = 1;
    const int total = FS * 10;
    for (int i0 = 0; i0 < total; i0 += BLK) {
        for (int k = 0; k < BLK; ++k) {
            L[k] = test_signal(i0 + k, 0); R[k] = test_signal(i0 + k, 1);
            KL[k] = L[k]; KR[k] = R[k];
        }
        if (i0 == FS / 2) { L[3] = NAN; R[5] = INFINITY; L[7] = -INFINITY; KL[2] = NAN; }
        ec_fx_process(&g_fx, &p, bpm, is_send ? NULL : KL, is_send ? NULL : KR, L, R, BLK);
        for (int k = 0; k < BLK; ++k) {
            if (!isfinite(L[k]) || !isfinite(R[k])) ok = 0;
            else { mx = fmaxf(mx, fabsf(L[k])); mx = fmaxf(mx, fabsf(R[k])); }
        }
    }
    *finite_ok = ok;
    return mx;
}

/* Flanger / phaser: the SC loops have no clip, only |fb| <= 0.9 -> a comb
 * peak / DC gain of up to 1/(1-0.9) = 10 on a full-scale input. Comp: MAKE
 * +12 dB on a 1.0 input = 4.0. Everything else stays well under 4. */
static float bound_for(int m)
{
    if (m == EC_FX_FLANGER) return 10.5f;
    if (m == EC_FX_PHASER) return 6.f;
    if (m == EC_FX_COMP) return 4.05f;
    if (m == EC_FX_REVERB) return 12.5f;    /* FreeVerb2 DC gain at ROOM max: 0.03*8/(1-0.98) = 12 */
    return 4.f;
}

static void test_extremes(void)
{
    printf("1. extremes (10 s noise+NaN / impulses / DC / sine, insert+send, min/max/random)\n");
    printf("   %-10s %8s %8s %8s %8s\n", "machine", "min", "max", "random", "send");
    for (int m = 1; m <= EC_FX_COUNT; ++m) {
        float r[4] = {0};
        int ok;
        r[0] = run_extreme(m, 0, 0, 300.f, &ok); CHECK(ok, "%s min: non-finite", ec_fx_name(m));
        r[1] = run_extreme(m, 0, 1, 120.f, &ok); CHECK(ok, "%s max: non-finite", ec_fx_name(m));
        for (int k = 0; k < 3; ++k) {
            float v = run_extreme(m, 0, 2, 40.f + 200.f * frand(), &ok);
            CHECK(ok, "%s random: non-finite", ec_fx_name(m));
            r[2] = fmaxf(r[2], v);
        }
        for (int mode = 0; mode < 3; ++mode) {
            float v = run_extreme(m, 1, mode, 120.f, &ok);
            CHECK(ok, "%s send: non-finite", ec_fx_name(m));
            r[3] = fmaxf(r[3], v);
        }
        printf("   %-10s %8.3f %8.3f %8.3f %8.3f\n", ec_fx_name(m), r[0], r[1], r[2], r[3]);
        float b = bound_for(m);
        for (int k = 0; k < 4; ++k) CHECK(r[k] < b, "%s max %.3f >= %.2f", ec_fx_name(m), r[k], b);
    }
}

/* ------------------------------------------------------------ 2. tails */

static int mem_subnormals(void)
{
    int c = 0;
    for (uint32_t i = 0; i < EC_FX_MEM_FLOATS; ++i) c += is_sub(g_mem[i]);
    return c;
}

/* 0.5 s noise burst (0.5), then silence. Returns seconds until the 1024-frame
 * block peak stays below -60 dBFS (-1 = never within maxs); *zero_s = first
 * block of exact zeros that stays zero; *peak_after = peak in [hold0, maxs). */
static float run_tail(ec_fx_params_t *p, float maxs, float *zero_s, int *subs, float hold0, float *peak_after)
{
    ec_fx_init(&g_fx, g_mem, FS, 0, 77u);
    memset(g_mem, 0, sizeof g_mem);
    float L[BLK], R[BLK];
    int total = (int)(maxs * FS), burst = FS / 2;
    int last_loud = 0, last_nonzero = 0, sub = 0;
    float pa = 0.f;
    for (int i0 = 0; i0 < total; i0 += BLK) {
        for (int k = 0; k < BLK; ++k) {
            float v = (i0 + k < burst) ? 0.5f * frand2() : 0.f;
            L[k] = v; R[k] = (i0 + k < burst) ? 0.5f * frand2() : 0.f;
        }
        ec_fx_process(&g_fx, p, 120.f, NULL, NULL, L, R, BLK);
        for (int k = 0; k < BLK; ++k) {
            float a = fmaxf(fabsf(L[k]), fabsf(R[k]));
            if (a >= 0.001f) last_loud = i0 + k;
            if (a != 0.f) last_nonzero = i0 + k;
            sub += is_sub(L[k]) + is_sub(R[k]);
            if (i0 + k >= (int)(hold0 * FS)) pa = fmaxf(pa, a);
        }
    }
    *subs = sub + mem_subnormals();
    *zero_s = last_nonzero + 1 < total - FS ? (float)(last_nonzero + 1 - burst) / FS : -1.f;
    if (peak_after) *peak_after = pa;
    return last_loud + 1 < total - FS ? (float)(last_loud + 1 - burst) / FS : -1.f;
}

static void test_tails(void)
{
    printf("2. tails (0.5 s noise burst at -6 dBFS, then silence; MIX 1)\n");
    struct { int m; const char *what; float maxs; } T[] = {
        {EC_FX_DELAY, "FB max, 1 beat @120", 240.f},
        {EC_FX_ECHO, "FB max, 1 beat, ping-pong", 240.f},
        {EC_FX_TAPE_ECHO, "FB max, 1 beat, SAT 0", 240.f},
        {EC_FX_REVERB, "SIZE max, DAMP 0", 60.f},
        {EC_FX_BLACKHOLE, "GRAV 0.95", 300.f},
        {EC_FX_BLACKHOLE, "GRAV 0.5 (default)", 120.f},
    };
    for (unsigned t = 0; t < sizeof T / sizeof T[0]; ++t) {
        ec_fx_params_t p;
        ec_fx_params_default(&p);
        p.machine = T[t].m; p.mix = 1.f;
        p.delay_feedback = 1.f; p.echo_mode = (T[t].m == EC_FX_ECHO);
        p.reverb_size = 1.f; p.reverb_damp = 0.f;
        p.bh_gravity = t == 4 ? 0.95f : 0.5f;
        p.tape_sat = 0.f;
        float z; int subs;
        float s = run_tail(&p, T[t].maxs, &z, &subs, 0.f, NULL);
        printf("   %-10s %-28s -60 dB after %6.2f s, exact 0 after %6.2f s, subnormals %d\n",
               ec_fx_name(T[t].m), T[t].what, s, z, subs);
        CHECK(s > 0.f, "%s tail never fell below -60 dB", ec_fx_name(T[t].m));
        if (t != 4)   /* GRAV 0.95: -60 dB takes ~3 min, -120 dB does not fit the run */
            CHECK(z > 0.f, "%s tail never reached exact 0", ec_fx_name(T[t].m));
        CHECK(subs == 0, "%s: %d subnormals", ec_fx_name(T[t].m), subs);
    }
    /* freeze: gravity max holds */
    {
        ec_fx_params_t p;
        ec_fx_params_default(&p);
        p.machine = EC_FX_BLACKHOLE; p.mix = 1.f; p.bh_gravity = 1.f;
        float z, pa; int subs;
        run_tail(&p, 30.f, &z, &subs, 25.f, &pa);
        printf("   BLACKHOLE  GRAV max (freeze)              peak 24.5..30 s after the burst %.3f\n", pa);
        CHECK(pa > 0.02f, "BLACKHOLE freeze did not hold (%.4f)", pa);
        CHECK(pa <= 0.9001f, "BLACKHOLE freeze exceeded the 0.9 loop limiter (%.4f)", pa);
        CHECK(subs == 0, "freeze: %d subnormals", subs);
    }
}

/* ------------------------------------------------------------ 3. swaps */

static float run_swaps(int is_send, int *fails, float *own_step)
{
    ec_fx_params_t p;
    ec_fx_params_default(&p);
    ec_fx_init(&g_fx, g_mem, FS, is_send, 99u);
    float L[BLK], R[BLK], prevL = 0.f, prevR = 0.f, mx = 0.f;
    const int per = (int)(0.3 * FS) / BLK * BLK;
    int i = 0, target = EC_FX_NONE;
    *fails = 0;
    float own = 0.f;
    /* order 1..18, then 2 rapid doubles, a reversal, and back to NONE */
    int seq[EC_FX_COUNT + 6], ns = 0;
    for (int m = 1; m <= EC_FX_COUNT; ++m) seq[ns++] = m;
    seq[ns++] = EC_FX_REVERB; seq[ns++] = EC_FX_DELAY; seq[ns++] = EC_FX_CHORUS; seq[ns++] = EC_FX_NONE;
    for (int s = 0; s < ns; ++s) {
        int m = seq[s];
        for (int j = 0; j < per; j += BLK, i += BLK) {
            if (j == 0) { p.machine = m; target = m; }
            if (s == ns - 3 && j == 64) { p.machine = EC_FX_ECHO; target = EC_FX_ECHO; }      /* double, 1.5 ms */
            if (s == ns - 2 && j == 160) { p.machine = EC_FX_DELAY; target = EC_FX_DELAY; }   /* reversal mid-fade */
            if (s == ns - 2 && j == 192) { p.machine = EC_FX_CHORUS; target = EC_FX_CHORUS; }
            for (int k = 0; k < BLK; ++k) {
                float v = 0.5f * (float)sin(2 * PI_D * 220.0 * (i + k) / FS);
                L[k] = v; R[k] = v;
            }
            ec_fx_process(&g_fx, &p, 120.f, L, R, L, R, BLK);
            for (int k = 0; k < BLK; ++k) {
                float d = fmaxf(fabsf(L[k] - prevL), fabsf(R[k] - prevR));
                if (g_fx.fdir != 0 || j < 2048) mx = fmaxf(mx, d); else own = fmaxf(own, d);
                prevL = L[k]; prevR = R[k];
            }
        }
        if (ec_fx_running(&g_fx) != target) (*fails)++;
    }
    *own_step = own;
    return mx;
}

static void test_swaps(void)
{
    printf("3. swaps (0.5 sine 220 Hz, machine change every 0.3 s through all 18 + doubles)\n");
    int f1, f2;
    float o1, o2;
    float s1 = run_swaps(0, &f1, &o1), s2 = run_swaps(1, &f2, &o2);
    printf("   insert: max step in/around swaps %.4f (steady %.4f), send: %.4f (steady %.4f); "
           "sine alone 0.0157\n", s1, o1, s2, o2);
    CHECK(f1 == 0 && f2 == 0, "fade did not reach the target (%d / %d)", f1, f2);
    CHECK(s1 < 0.1f, "insert swap step %.4f", s1);
    CHECK(s2 < 0.1f, "send swap step %.4f", s2);
}

/* ------------------------------------------------------------ 4. dynamics */

static void test_dynamics(void)
{
    printf("4. dynamics\n");
    ec_fx_params_t p;
    float L[BLK], R[BLK], KL[BLK], KR[BLK];
    /* DUCK: steady 0.5 sine; key = full-scale 100 Hz sine 1..2 s */
    ec_fx_params_default(&p);
    p.machine = EC_FX_DUCK;
    ec_fx_init(&g_fx, g_mem, FS, 1, 5u);
    double e_pre = 0, e_duck = 0, e_post = 0; int n_pre = 0, n_duck = 0, n_post = 0;
    ec_fx_meter_t mt; float gr_mid = 0.f;
    for (int i = 0; i < FS * 4; i += BLK) {
        for (int k = 0; k < BLK; ++k) {
            float t = (float)(i + k) / FS;
            L[k] = R[k] = 0.5f * (float)sin(2 * PI_D * 440.0 * t);
            KL[k] = KR[k] = (t >= 1.f && t < 2.f) ? (float)sin(2 * PI_D * 100.0 * t) : 0.f;
        }
        ec_fx_process(&g_fx, &p, 120.f, KL, KR, L, R, BLK);
        float t0 = (float)i / FS;
        for (int k = 0; k < BLK; ++k) {
            if (t0 > 0.5f && t0 < 1.f) { e_pre += L[k] * L[k]; n_pre++; }
            if (t0 > 1.3f && t0 < 2.f) { e_duck += L[k] * L[k]; n_duck++; }
            if (t0 > 3.f) { e_post += L[k] * L[k]; n_post++; }
        }
        if (i == (int)(1.5f * FS) / BLK * BLK) { ec_fx_meter(&g_fx, &mt); gr_mid = mt.gr_db; CHECK(mt.valid && mt.thresh_db == -99.f, "duck meter"); }
    }
    float ratio = (float)sqrt((e_duck / n_duck) / (e_pre / n_pre));
    float rec = (float)sqrt((e_post / n_post) / (e_pre / n_pre));
    printf("   DUCK  AMNT %.3f: ducked level x%.3f (expect %.3f), meter %.2f dB, recovered x%.3f\n",
           p.duck_amount, ratio, 1.f - p.duck_amount, gr_mid, rec);
    CHECK(fabsf(ratio - (1.f - p.duck_amount)) < 0.05f, "duck depth %.3f", ratio);
    CHECK(rec > 0.98f, "duck recovery %.3f", rec);

    /* COMP: loud input shows gain reduction */
    ec_fx_params_default(&p);
    p.machine = EC_FX_COMP;
    ec_fx_init(&g_fx, g_mem, FS, 0, 6u);
    for (int i = 0; i < FS; i += BLK) {
        for (int k = 0; k < BLK; ++k) L[k] = R[k] = 0.9f * (float)sin(2 * PI_D * 200.0 * (i + k) / FS);
        ec_fx_process(&g_fx, &p, 120.f, NULL, NULL, L, R, BLK);
    }
    ec_fx_meter(&g_fx, &mt);
    printf("   COMP  0.9 sine: GR %.2f dB, key %.2f dB, thresh %.2f dB\n", mt.gr_db, mt.key_db, mt.thresh_db);
    CHECK(mt.valid && mt.gr_db < -1.f, "comp meter gr %.2f", mt.gr_db);

    /* LIMIT: GAIN max into the ceiling never exceeds it after the lookahead */
    for (int rel = 0; rel < 3; ++rel) {
        ec_fx_params_default(&p);
        p.machine = EC_FX_LIMIT; p.limit_gain = 1.f; p.limit_release = rel * 0.5f;
        p.limit_ceil = rel == 1 ? 0.f : 124.f / 127.f;
        ec_fx_init(&g_fx, g_mem, FS, 0, 7u);
        float ceil = powf(10.f, (-12.f + 12.f * p.limit_ceil) / 20.f), mx = 0.f;
        for (int i = 0; i < FS * 3; i += BLK) {
            for (int k = 0; k < BLK; ++k) {
                float t = (float)(i + k) / FS;
                float env = (t > 1.f && t < 1.002f) ? 1.f : 0.3f;
                L[k] = env * (0.6f * (float)sin(2 * PI_D * 150.0 * t) + 0.4f * frand2());
                R[k] = (i + k) % 3000 == 0 ? 1.f : 0.5f * frand2();
            }
            ec_fx_process(&g_fx, &p, 120.f, NULL, NULL, L, R, BLK);
            if (i >= FS / 4)        /* past the start: the dry passes while the lookahead primes */
                for (int k = 0; k < BLK; ++k) mx = fmaxf(mx, fmaxf(fabsf(L[k]), fabsf(R[k])));
        }
        ec_fx_meter(&g_fx, &mt);
        printf("   LIMIT REL %.1f ceil %.4f: max out %.6f (+%.2e), GR %.2f dB\n",
               p.limit_release, ceil, mx, mx - ceil, mt.gr_db);
        CHECK(mx <= ceil * 1.0001f, "limit exceeded: %.6f > %.6f", mx, ceil);
    }
}

/* ------------------------------------------------------------ 5. unity */

static float run_unity(ec_fx_params_t *p, int is_send, int skip)
{
    ec_fx_init(&g_fx, g_mem, FS, is_send, 8u);
    float L[BLK], R[BLK], X[BLK], Y[BLK], mx = 0.f;
    for (int i = 0; i < FS * 2; i += BLK) {
        for (int k = 0; k < BLK; ++k) {
            X[k] = L[k] = 0.7f * frand2();
            Y[k] = R[k] = 0.3f * (float)sin(2 * PI_D * 97.0 * (i + k) / FS) + 0.2f * frand2();
        }
        ec_fx_process(&g_fx, p, 120.f, NULL, NULL, L, R, BLK);
        if (i >= skip)
            for (int k = 0; k < BLK; ++k) mx = fmaxf(mx, fmaxf(fabsf(L[k] - X[k]), fabsf(R[k] - Y[k])));
    }
    return mx;
}

static void test_unity(void)
{
    printf("5. unity\n");
    ec_fx_params_t p;
    ec_fx_params_default(&p);
    float d = run_unity(&p, 0, 0);
    CHECK(d == 0.f, "NONE insert not bit-exact (%g)", d);
    p.machine = EC_FX_DJ_EQ;
    d = run_unity(&p, 0, 2048);
    printf("   DJ EQ 0/0/0 max |y-x| %g\n", d);
    CHECK(d < 1e-6f, "DJ EQ unity %g", d);
    ec_fx_params_default(&p);
    p.machine = EC_FX_MOTION;
    d = run_unity(&p, 0, 2048);
    printf("   MOTION 0/0/0 max |y-x| %g\n", d);
    CHECK(d == 0.f, "MOTION unity %g", d);
    float worst = 0.f;
    for (int m = 1; m <= EC_FX_COUNT; ++m) {
        if (m == EC_FX_NONE || m == EC_FX_DESTROY || m == EC_FX_DJ_EQ || m == EC_FX_DUCK ||
            m == EC_FX_MOTION || m == EC_FX_LIMIT) continue;
        ec_fx_params_default(&p);
        p.machine = m; p.mix = 0.f;
        d = run_unity(&p, 0, 4096);
        worst = fmaxf(worst, d);
        CHECK(d == 0.f, "%s MIX 0 != dry (%g)", ec_fx_name(m), d);
    }
    printf("   MIX 0 on the 12 wet machines: max |y-dry| %g\n", worst);
}

/* ------------------------------------------------------------ extra: FreqShift */

static double goertzel(const float *x, int n, double f)
{
    double w = 2 * PI_D * f / FS, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) { double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return s1 * s1 + s2 * s2 - c * s1 * s2;
}

static void test_freqshift(void)
{
    ec_fx_params_t p;
    ec_fx_params_default(&p);
    p.machine = EC_FX_RINGS; p.rings_mode = 1; p.mix = 1.f; p.modfx_tone = 1.f;
    p.rings_freq = 0.001f + 0.999f * (float)(log(100.0) / log(2000.0));     /* +100 Hz */
    ec_fx_init(&g_fx, g_mem, FS, 0, 9u);
    static float out[FS];
    float L[BLK], R[BLK];
    for (int i = 0; i < FS; i += BLK) {
        for (int k = 0; k < BLK; ++k) L[k] = R[k] = 0.5f * (float)sin(2 * PI_D * 1000.0 * (i + k) / FS);
        ec_fx_process(&g_fx, &p, 120.f, NULL, NULL, L, R, BLK);
        memcpy(out + i, L, sizeof L);
    }
    double up = goertzel(out + FS / 2, FS / 2, 1100.0), dn = goertzel(out + FS / 2, FS / 2, 900.0);
    double db = 10 * log10(up / (dn + 1e-30));
    printf("   RINGS SHIFT +100 Hz on 1 kHz: 1100 Hz vs 900 Hz %+.1f dB\n", db);
    CHECK(db > 25.0, "freqshift sideband rejection %.1f dB", db);
}

/* ------------------------------------------------------------ 6. bench */

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + 1e-9 * (double)ts.tv_nsec;
}

static void bench(void)
{
    printf("6. benchmark (native, ns per stereo frame, 16-frame calls, default params, MIX 1)\n   ");
    for (int m = 1; m <= EC_FX_COUNT; ++m) {
        ec_fx_params_t p;
        ec_fx_params_default(&p);
        p.machine = m; p.mix = 1.f;
        if (m == EC_FX_RINGS) p.rings_mode = 1;
        if (m == EC_FX_DESTROY) { p.drive = 0.5f; p.destroy_fold = 0.5f; p.destroy_sat = 0.5f; p.destroy_crush = 0.5f; p.destroy_srr = 0.5f; p.destroy_warble = 0.5f; }
        if (m == EC_FX_CASSETTE) { p.cass_crackle = 0.5f; p.cass_drop = 0.5f; }
        ec_fx_init(&g_fx, g_mem, FS, 0, 3u);
        static float buf[2][FS];
        for (int i = 0; i < FS; ++i) { buf[0][i] = 0.5f * frand2(); buf[1][i] = 0.5f * frand2(); }
        const int secs = 3;
        double t0 = now_s();
        for (int s = 0; s < secs; ++s)
            for (int i = 0; i + BLK <= FS; i += BLK) {
                float L[BLK], R[BLK];
                memcpy(L, buf[0] + i, sizeof L); memcpy(R, buf[1] + i, sizeof R);
                ec_fx_process(&g_fx, &p, 120.f, buf[0] + i, buf[1] + i, L, R, BLK);
            }
        double ns = (now_s() - t0) * 1e9 / ((double)secs * FS);
        printf("%-9s %6.1f%s", ec_fx_name(m), ns, (m % 6 == 0) ? "\n   " : "  ");
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    (void)argv;
    test_extremes();
    test_tails();
    test_swaps();
    test_dynamics();
    test_unity();
    test_freqshift();
    if (argc < 2) bench();
    printf("test_fx: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
