/*
 * test_e4.c -- E4 through the whole engine: the send buses sum, the master
 * insert, DUCK keyed by the tracks, MOD reaching its destinations, an insert
 * swapped during playback, the idle CPU saver, panic clearing the tails.
 * Exit code 0 = all passed.
 */
#include "ec_testkit.h"
#include "../../src/dsp/ec_internal.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define SEC TK_FS

static void render(ec_dsp_t *d, float *out, int frames)
{
    for (int pos = 0; pos < frames; pos += 128) {
        int n = frames - pos < 128 ? frames - pos : 128;
        ec_dsp_process(d, out + 2 * pos, n);
    }
}

/* track 0 droning a 220 Hz sine loop (amplitude amp), transport running */
static ec_dsp_t *sine_engine(float amp, int16_t **keep)
{
    ec_dsp_t *d = tk_engine();
    int16_t *sn = tk_sine(220.f, amp, SEC);
    ec_sample_t s = {sn, SEC, (float)TK_FS, 1, 0};
    ec_dsp_set_sample(d, 0, &s);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    *keep = sn;
    return d;
}

static void set_tp(ec_dsp_t *d, int t, void (*f)(ec_track_params_t *))
{
    ec_track_params_t p = *ec_dsp_get_track_params(d, t);
    f(&p);
    ec_dsp_track_params(d, t, &p);
}

/* ---------------------------------------------------------------- sends */
static void sp_send1(ec_track_params_t *p) { p->send1_level = 1.f; }
static void sp_send12(ec_track_params_t *p) { p->send1_level = 1.f; p->send2_level = 0.5f; }
static void sp_send1_mute(ec_track_params_t *p) { p->send1_level = 1.f; p->mute = 1; }
static void sp_send1_post_lofi(ec_track_params_t *p)
{
    p->send1_level = 1.f; p->send_tap = 2; p->fx.machine = EC_FX_LOFI; p->fx.mix = 1.f;
    p->fx.lofi_bits = 3.f;
}

static void unity_bus(ec_dsp_t *d, int b)
{
    ec_fx_params_t fp;
    ec_fx_params_default(&fp);
    fp.machine = EC_FX_DJ_EQ;                 /* 0/0/0 = exact unity, always wet */
    ec_dsp_bus_params(d, b, &fp);
}

static double ratio_after(const float *o, const float *ref, int from, int n)
{
    double num = 0, den = 0;
    for (int i = from; i < n; ++i) { num += (double)o[2 * i] * ref[2 * i]; den += (double)ref[2 * i] * ref[2 * i]; }
    return num / den;
}

static double max_dev(const float *o, const float *ref, double k, int from, int n)
{
    double e = 0;
    for (int i = from; i < n; ++i) { double x = fabs(o[2 * i] - k * ref[2 * i]); if (x > e) e = x; }
    return e;
}

static void test_sends(void)
{
    printf("sends: S1 / S2 returns sum onto the master\n");
    const int n = SEC;
    float *ref = malloc(n * 2 * sizeof(float)), *o = malloc(n * 2 * sizeof(float));
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.05f, &k1);
    render(d, ref, n);
    free(d); free(k1);

    /* S1 at level 1, tap pre: the send carries the post-filter signal (before the mix's centre 0.707) */
    d = sine_engine(0.05f, &k1);
    unity_bus(d, 0);
    set_tp(d, 0, sp_send1);
    render(d, o, n);
    double r = ratio_after(o, ref, SEC / 4, n), want = 1.0 + 1.0 / 0.70710678;
    CHECK(fabs(r - want) < 1e-3, "S1 level 1: x%.4f, want %.4f", r, want);
    CHECK(max_dev(o, ref, want, SEC / 4, n) < 1e-4, "S1 sample error %g", max_dev(o, ref, want, SEC / 4, n));
    printf("  S1 x%.4f\n", r);
    free(d); free(k1);

    /* S1 1 + S2 0.5 */
    d = sine_engine(0.05f, &k1);
    unity_bus(d, 0); unity_bus(d, 1);
    set_tp(d, 0, sp_send12);
    render(d, o, n);
    r = ratio_after(o, ref, SEC / 4, n);
    want = 1.0 + 1.5 / 0.70710678;
    CHECK(fabs(r - want) < 1e-3, "S1 + S2: x%.4f, want %.4f", r, want);
    printf("  S1 + S2/2 x%.4f\n", r);
    free(d); free(k1);

    /* a muted track still feeds its sends (the SC send tap sits before \elasticatTrackMix's mute) */
    d = sine_engine(0.05f, &k1);
    unity_bus(d, 0);
    set_tp(d, 0, sp_send1_mute);
    render(d, o, n);
    r = ratio_after(o, ref, SEC / 4, n);
    CHECK(fabs(r - 1.0 / 0.70710678) < 1e-3, "muted track's send: x%.4f", r);
    free(d); free(k1);

    /* S1 machine NONE: a send return adds nothing (spawnSend1: no synth) */
    d = sine_engine(0.05f, &k1);
    set_tp(d, 0, sp_send1);
    render(d, o, n);
    r = ratio_after(o, ref, SEC / 4, n);
    CHECK(fabs(r - 1.0) < 1e-4, "S1 NONE adds nothing: x%.4f", r);
    free(d); free(k1);

    /* tap post: the send hears the insert (LOFI 3 bits) */
    d = sine_engine(0.05f, &k1);
    unity_bus(d, 0);
    set_tp(d, 0, sp_send1_post_lofi);
    render(d, o, n);
    {
        int levels = 0; float seen[64]; int ns = 0;
        for (int i = SEC / 2; i < n && ns < 64; ++i) {
            int found = 0;
            for (int k = 0; k < ns; ++k) if (fabsf(seen[k] - o[2 * i]) < 1e-6f) { found = 1; break; }
            if (!found) seen[ns++] = o[2 * i];
        }
        levels = ns;
        CHECK(levels < 20, "post tap carries the crushed insert: %d output levels", levels);
    }
    free(d); free(k1);
    free(ref); free(o);
}

/* ---------------------------------------------------------------- master insert */
static void test_master(void)
{
    printf("master insert: LIMIT ceiling, DJ EQ kill\n");
    const int n = SEC;
    float *o = malloc(n * 2 * sizeof(float));
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.5f, &k1);
    ec_fx_params_t fp;
    ec_fx_params_default(&fp);
    fp.machine = EC_FX_LIMIT;
    fp.limit_ceil = 0.f;                      /* -12 dB */
    fp.limit_gain = 0.5f;                     /* +9 dB */
    ec_dsp_bus_params(d, 2, &fp);
    render(d, o, n);
    float pk = 0;
    for (int i = SEC / 2; i < n; ++i) pk = fmaxf(pk, fmaxf(fabsf(o[2 * i]), fabsf(o[2 * i + 1])));
    CHECK(pk <= 0.2512f * 1.002f && pk > 0.2f, "LIMIT -12 dB on the master: peak %.4f", pk);
    printf("  limit peak %.4f (ceiling 0.2512)\n", pk);
    ec_bus_status_t bs;
    ec_dsp_bus_status(d, 2, &bs);
    CHECK(bs.fx_running == EC_FX_LIMIT && bs.fx_meter.valid && bs.fx_meter.gr_db < -3.f,
          "master meter: running %d gr %.1f dB", bs.fx_running, bs.fx_meter.gr_db);
    fp.machine = EC_FX_DJ_EQ;
    fp.eq_low = fp.eq_mid = fp.eq_high = -1.f;
    ec_dsp_bus_params(d, 2, &fp);
    render(d, o, n);
    pk = 0;
    for (int i = SEC / 2; i < n; ++i) pk = fmaxf(pk, fabsf(o[2 * i]));
    CHECK(pk < 1e-4f, "DJ EQ full kill on the master: %g", pk);
    free(d); free(k1); free(o);
}

/* ---------------------------------------------------------------- duck */
static void sp_duck_src(ec_track_params_t *p) { p->send1_level = 1.f; p->mute = 1; }
static void sp_key_off(ec_track_params_t *p) { p->amp = 0.f; }
static void sp_key_on(ec_track_params_t *p) { p->amp = 0.8f; }

static void test_duck(void)
{
    printf("duck: S1 DUCK keyed by the tracks' mix\n");
    const int n = SEC;
    float *o = malloc(n * 2 * sizeof(float));
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.3f, &k1);
    int16_t *k2 = tk_sine(80.f, 0.8f, SEC);
    ec_sample_t s2 = {k2, SEC, (float)TK_FS, 1, 0};
    ec_dsp_set_sample(d, 1, &s2);
    ec_dsp_note_on(d, 0, 1, 0);
    set_tp(d, 0, sp_duck_src);                 /* track 0 only feeds S1 */
    set_tp(d, 1, sp_key_off);
    ec_fx_params_t fp;
    ec_fx_params_default(&fp);
    fp.machine = EC_FX_DUCK;
    fp.duck_amount = 0.6f;
    fp.duck_sens = 1.f;
    ec_dsp_bus_params(d, 0, &fp);
    render(d, o, n);
    ec_bus_status_t a, b;
    ec_dsp_bus_status(d, 0, &a);
    double ra = 0;
    for (int i = n / 2; i < n; ++i) ra += o[2 * i] * o[2 * i];
    set_tp(d, 1, sp_key_on);
    render(d, o, n);
    ec_dsp_bus_status(d, 0, &b);
    CHECK(a.fx_meter.valid && a.fx_meter.gr_db > -0.5f, "no key: gr %.2f dB", a.fx_meter.gr_db);
    CHECK(b.fx_meter.gr_db < -6.f && b.fx_meter.gr_db > -10.f, "keyed: gr %.2f dB (amount 0.6 = -8 dB)",
          b.fx_meter.gr_db);
    CHECK(b.peak_l < a.peak_l * 0.6f, "the return dips: %.3f -> %.3f", a.peak_l, b.peak_l);
    printf("  gr %.2f -> %.2f dB, return peak %.3f -> %.3f (rms before %.3f)\n", a.fx_meter.gr_db, b.fx_meter.gr_db,
           a.peak_l, b.peak_l, sqrt(ra / (n / 2)));
    free(d); free(k1); free(k2); free(o);
}

/* ---------------------------------------------------------------- MOD */
static void sp_lfo_pan(ec_track_params_t *p)
{
    p->mod.slot[0].src = 1; p->mod.slot[0].bus = EC_MOD_PAN; p->mod.slot[0].depth = 1.f; p->mod.lfo[0].wave = EC_LW_SINE; p->mod.lfo[0].depth = 1.f;
    p->mod.lfo[0].beats = 1.f;
}
static void sp_lfo_cut_macro(ec_track_params_t *p)
{
    p->mod.slot[0].src = 1; p->mod.slot[0].bus = EC_MOD_CUTOFF; p->mod.slot[0].depth = 0.3f; p->mod.lfo[0].wave = EC_LW_SQR; p->mod.lfo[0].depth = -1.f;
    p->mod.lfo[0].mode = EC_LM_TRIG;
    p->mod.macro_base[1] = 0.5f;
    p->mod.slot[1].src = 9; p->mod.slot[1].bus = EC_MOD_AMP; p->mod.slot[1].depth = -1.f;   /* M2 -> AMP */
}
static void sp_menv_res(ec_track_params_t *p)
{
    p->mod.slot[0].src = 3; p->mod.slot[0].bus = EC_MOD_RES; p->mod.slot[0].depth = 1.f; p->mod.menv_depth = 1.f; p->mod.menv_attack = 0.01f;
    p->mod.menv_decay = 0.1f; p->mod.menv_sustain = 0.5f; p->mod.menv_release = 0.05f;
}
static void sp_vel(ec_track_params_t *p)
{
    p->machine = EC_MACH_GRID_SLICE; p->mod.slot[0].src = 4; p->mod.slot[0].bus = EC_MOD_PITCH; p->mod.slot[0].depth = 1.f; p->mod.mvel_depth = 1.f;
}

static void test_mod(void)
{
    printf("mod: LFO -> pan / cutoff, macros, mod env, velocity\n");
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.2f, &k1);
    set_tp(d, 0, sp_lfo_pan);
    float buf[2 * 16];
    float mn = 9, mx = -9;
    double lw[40] = {0}, rw[40] = {0};
    for (int b = 0; b < SEC / 16; ++b) {
        ec_dsp_process(d, buf, 16);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        mn = fminf(mn, st.mod[EC_MOD_PAN]); mx = fmaxf(mx, st.mod[EC_MOD_PAN]);
        int w = b * 16 / (SEC / 40);
        for (int i = 0; i < 16 && w < 40; ++i) { lw[w] += buf[2 * i] * buf[2 * i]; rw[w] += buf[2 * i + 1] * buf[2 * i + 1]; }
    }
    CHECK(mn < -0.98f && mx > 0.98f, "LFO 1 -> PAN swings %.3f .. %.3f", mn, mx);
    int left = 0, right = 0;
    for (int w = 4; w < 40; ++w) { if (lw[w] > 4 * rw[w]) left++; if (rw[w] > 4 * lw[w]) right++; }
    CHECK(left >= 4 && right >= 4, "the output pans both ways: %d left-heavy, %d right-heavy windows", left, right);
    printf("  pan %.3f..%.3f, %d/%d windows L/R heavy\n", mn, mx, left, right);

    /* square LFO (TRIG mode, depth -1) -> CUTOFF is the x0.3 term; macro 2 at 0.5 -> AMP -0.5 */
    set_tp(d, 0, sp_lfo_cut_macro);
    ec_event_t e = tk_ev(0, EC_EV_MOD_TRIG, 0);
    e.flags = EC_TF_LFO;
    ec_dsp_event(d, &e);
    ec_dsp_process(d, buf, 16);
    ec_track_status_t st;
    ec_dsp_track_status(d, 0, &st);
    CHECK(fabsf(st.mod[EC_MOD_CUTOFF] + 0.3f) < 1e-4f, "TRIG square at phase 0, depth -1: cutoff %.4f", st.mod[EC_MOD_CUTOFF]);
    CHECK(fabsf(st.mod[EC_MOD_AMP] + 0.5f) < 1e-4f && fabsf(st.macro[1] - 0.5f) < 1e-4f, "macro 2 -> AMP %.4f",
          st.mod[EC_MOD_AMP]);
    /* the amp mod halves the level */
    {
        float o[2 * 4096];
        double r1 = 0, r0 = 0;
        render(d, o, 4096);
        for (int i = 2048; i < 4096; ++i) r1 += o[2 * i] * o[2 * i];
        free(d); free(k1);
        d = sine_engine(0.2f, &k1);
        render(d, o, 4096);
        for (int i = 2048; i < 4096; ++i) r0 += o[2 * i] * o[2 * i];
        CHECK(fabs(sqrt(r1 / r0) - 0.5) < 0.03, "AMP mod -0.5 -> level x%.3f", sqrt(r1 / r0));
    }

    /* mod env -> RES: attack to 1, decay to the sustain, release on note off */
    set_tp(d, 0, sp_menv_res);
    e = tk_ev(0, EC_EV_MOD_TRIG, 0);
    e.flags = EC_TF_MENV;
    e.length = 0;
    ec_dsp_event(d, &e);
    float peak = 0, late = 0;
    for (int b = 0; b < SEC / 2 / 16; ++b) {
        ec_dsp_process(d, buf, 16);
        ec_dsp_track_status(d, 0, &st);
        peak = fmaxf(peak, st.mod[EC_MOD_RES]);
        late = st.mod[EC_MOD_RES];
    }
    CHECK(peak > 0.97f && fabsf(late - 0.5f) < 0.02f, "mod env: peak %.3f, sustain %.3f", peak, late);
    e = tk_ev(0, EC_EV_NOTE_OFF, 0);
    ec_dsp_event(d, &e);
    for (int b = 0; b < SEC / 4 / 16; ++b) ec_dsp_process(d, buf, 16);
    ec_dsp_track_status(d, 0, &st);
    CHECK(fabsf(st.mod[EC_MOD_RES]) < 0.01f, "mod env released: %.4f", st.mod[EC_MOD_RES]);
    free(d); free(k1);

    /* velocity: the latched slice velocity, Lag 0.01 */
    d = sine_engine(0.2f, &k1);
    set_tp(d, 0, sp_vel);
    e = tk_ev(0, EC_EV_SLICE_ON, 0);
    e.slice = 1;
    e.velocity = 0.4f;
    ec_dsp_event(d, &e);
    for (int b = 0; b < 100; ++b) ec_dsp_process(d, buf, 16);
    ec_dsp_track_status(d, 0, &st);
    CHECK(fabsf(st.mod[EC_MOD_PITCH] - 0.4f) < 0.01f, "velocity 0.4 -> PITCH %.3f", st.mod[EC_MOD_PITCH]);
    free(d); free(k1);
}

/* ---------------------------------------------------------------- swap during play */
static int g_mach;
static void sp_mach(ec_track_params_t *p) { p->fx.machine = g_mach; }

static void test_swap(void)
{
    printf("insert swap during playback: no clicks\n");
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.2f, &k1);
    const int seg = (int)(0.3 * SEC), n = seg * 19;
    float *o = malloc(n * 2 * sizeof(float));
    for (int k = 0; k < 19; ++k) {
        g_mach = 1 + (k % 18);
        set_tp(d, 0, sp_mach);
        render(d, o + 2 * k * seg, seg);
    }
    /* steady step per segment (away from the swap) vs the step around each swap */
    double worst = 0;
    int bad = 0;
    for (int k = 1; k < 19; ++k) {
        double steady = 0, around = 0;
        int s0 = k * seg;
        for (int i = s0 - seg + 2000; i < s0 - 100; ++i) steady = fmax(steady, fabs(o[2 * i] - o[2 * i - 2]));
        for (int i = s0 + 3000; i < s0 + seg - 100; ++i) steady = fmax(steady, fabs(o[2 * i] - o[2 * i - 2]));
        for (int i = s0; i < s0 + 3000; ++i) around = fmax(around, fabs(o[2 * i] - o[2 * i - 2]));
        double excess = around - steady;
        if (excess > worst) worst = excess;
        if (around > steady * 1.25 + 0.01) { bad++; printf("  swap to %d: step %.4f vs steady %.4f\n", 1 + k % 18, around, steady); }
    }
    CHECK(bad == 0, "%d swaps stepped above the machines' own steady steps", bad);
    printf("  worst step excess at a swap: %.4f\n", worst);
    float fin = 0;
    for (int i = 0; i < 2 * n; ++i) if (!isfinite(o[i])) fin = 1;
    CHECK(fin == 0, "finite");
    free(o); free(d); free(k1);
}

/* ---------------------------------------------------------------- idle + panic */
static void sp_delay_tail(ec_track_params_t *p) { p->fx.machine = EC_FX_DELAY; p->fx.delay_feedback = 0.9f; p->fx.mix = 0.5f; }

static void test_idle(void)
{
    printf("idle CPU saver: a stopped silent track pauses, an event wakes it, tails keep it awake\n");
    int16_t *k1;
    ec_dsp_t *d = sine_engine(0.2f, &k1);
    float buf[2 * 128];
    ec_track_status_t st;
    for (int b = 0; b < 5 * SEC / 128; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 0, &st);
    CHECK(!st.idle, "playing: awake");
    ec_dsp_transport(d, 0, 0, 0);
    int t_idle = -1;
    ec_dsp_track_status(d, 1, &st);
    for (int b = 0; b < 6 * SEC / 128; ++b) {
        ec_dsp_process(d, buf, 128);
        ec_dsp_track_status(d, 0, &st);
        if (st.idle && t_idle < 0) t_idle = b * 128;
    }
    CHECK(t_idle > 4 * SEC && t_idle < 5 * SEC, "stopped track 0 idles after %.2f s", t_idle / (double)SEC);
    ec_dsp_track_status(d, 3, &st);
    CHECK(st.idle, "an empty track idles");
    ec_dsp_note_on(d, 0, 0, 0);
    ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 0, &st);
    CHECK(!st.idle, "a note wakes it");
    free(d); free(k1);

    /* a ringing delay tail keeps the track awake past the grace; panic (double stop) clears it */
    d = sine_engine(0.2f, &k1);
    set_tp(d, 0, sp_delay_tail);
    for (int b = 0; b < SEC / 128; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_transport(d, 0, 0, 0);
    ec_event_t e = tk_ev(0, EC_EV_NOTE_OFF, 0);
    ec_dsp_event(d, &e);
    for (int b = 0; b < 3 * SEC / 128; ++b) ec_dsp_process(d, buf, 128);
    float pk = 0;
    for (int i = 0; i < 256; ++i) pk = fmaxf(pk, fabsf(buf[i]));
    ec_dsp_track_status(d, 0, &st);
    CHECK(!st.idle && pk > 0.002f, "the delay tail rings (%.4f) and keeps the track awake", pk);
    e = tk_ev(0, EC_EV_PANIC, -1);
    ec_dsp_event(d, &e);
    for (int b = 0; b < 4; ++b) ec_dsp_process(d, buf, 128);
    pk = 0;
    for (int i = 0; i < 256; ++i) pk = fmaxf(pk, fabsf(buf[i]));
    CHECK(pk < 1e-5f, "panic cleared the tail: %g", pk);
    free(d); free(k1);
}

int main(void)
{
    test_sends();
    test_master();
    test_duck();
    test_mod();
    test_swap();
    test_idle();
    printf("test_e4: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
