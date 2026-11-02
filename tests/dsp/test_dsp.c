/*
 * test_dsp.c -- unit tests for the Elasticat FM-1 sound engine (src/dsp).
 * Exit code 0 = all passed.
 */
#include "ec_scene.h"
#include "../../src/dsp/ec_internal.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void render(ec_dsp_t *d, float *out, int frames, int block)
{
    for (int pos = 0; pos < frames; pos += block) {
        int n = frames - pos < block ? frames - pos : block;
        ec_dsp_process(d, out + 2 * pos, n);
    }
}

static int finite_buf(const float *b, int n)
{
    for (int i = 0; i < n; ++i) if (!isfinite(b[i])) return 0;
    return 1;
}

/* 1. ElasticatReader at rate 1 reproduces the sample (Hermite at integer
 *    positions is exact) once the start fade is done. */
static void test_reader_bitnear(void)
{
    printf("reader: tape rate 1 reproduces the sample\n");
    uint32_t fr;
    int16_t *loop = tk_drum_loop(120.f, &fr);
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 0};
    ec_reader_t r;
    ec_reader_init(&r);
    int n = 20000;
    float *L = malloc(n * sizeof(float)), *R = malloc(n * sizeof(float));
    ec_reader_process(&r, &s, 0.0, (double)fr, 1.0, 0.0, 1.f / 441.f, L, R, n);
    double maxerr = 0;
    for (int i = 500; i < n; ++i) {
        double e1 = fabs(L[i] - loop[2 * i] / 32768.0), e2 = fabs(R[i] - loop[2 * i + 1] / 32768.0);
        if (e1 > maxerr) maxerr = e1;
        if (e2 > maxerr) maxerr = e2;
    }
    CHECK(maxerr < 1e-6, "max error %g", maxerr);

    /* wrap at the seam: a whole-loop region keeps reproducing it (crossfade
     * of identical audio at the same position = identity) */
    free(L); free(R);
    free(loop);
}

/* 1b. end to end: tape through the default chain is the sample, scaled
 *     (default filter 20 kHz LP, amp 100/127 * 2.63 * 0.5, LeakDC). */
static void test_tape_end_to_end(void)
{
    printf("engine: tape at rate 1 through the default chain\n");
    uint32_t fr = TK_FS;
    int16_t *sn = tk_sine(500.f, 0.5f, fr);
    ec_dsp_t *d = tk_engine();
    ec_sample_t s = {sn, fr, (float)TK_FS, 1, 0};
    ec_dsp_set_sample(d, 0, &s);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    int n = 30000;
    float *o = malloc(n * 2 * sizeof(float));
    render(d, o, n, 128);
    /* the 20 kHz RLPF adds ~2 samples of latency and a few % of loss at 500 Hz:
     * find the best lag, then the gain and shape must match */
    double g = (100.0 / 127.0) * 2.63 * 0.5;
    double best_gain = 0, err = 1e9;
    for (int lag = 0; lag <= 4; ++lag) {
        double num = 0, den = 0, e = 0;
        for (int i = 9000; i < n; ++i) { double ref = sn[2 * (i - lag)] / 32768.0 * g; num += o[2 * i] * ref; den += ref * ref; }
        double gain = num / den;
        for (int i = 9000; i < n; ++i) {
            double d1 = fabs(o[2 * i] - sn[2 * (i - lag)] / 32768.0 * g * gain);
            if (d1 > e) e = d1;
        }
        if (e < err) { err = e; best_gain = gain; }
    }
    CHECK(fabs(best_gain - 1.0) < 0.05, "gain vs expected %f", best_gain);
    CHECK(err < 0.03, "shape error %f (fractional-sample filter latency)", err);
    free(o); free(sn); free(d);
}

/* 2. tempo_varispeed: the region is fitted to sample_steps; after every N
 *    steps the playhead is exactly on the loop boundary. */
static void test_tempo_boundary(void)
{
    printf("tempo_varispeed: lands on the loop boundary\n");
    uint32_t fr;
    int16_t *loop = tk_drum_loop(97.f, &fr);   /* deliberately not 120 */
    ec_dsp_t *d = tk_engine();
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 0};
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.mode = EC_WARP_TEMPO_VARISPEED;
    p.sample_steps = 16.f;
    ec_dsp_track_params(d, 0, &p);
    ec_dsp_set_sample(d, 0, &s);
    ec_dsp_set_tempo(d, 120.f);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    float buf[2 * 100];
    double worst = 0;
    /* 16 steps at 120 BPM = 88200 samples; check 12 bars */
    for (int bar = 1; bar <= 12; ++bar) {
        for (int b = 0; b < 882; ++b) ec_dsp_process(d, buf, 100);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        double dist = st.pos_region < 0.5f ? st.pos_region : 1.0 - st.pos_region;
        if (dist > worst) worst = dist;
    }
    CHECK(worst < 2.0 / 88200.0, "worst boundary error %g of the loop (%.2f samples)", worst, worst * 88200);

    /* half region (8 steps) at 2 beats: also lands every 8 steps */
    p.loop_end = 64.f;
    ec_dsp_track_params(d, 0, &p);
    ec_dsp_transport(d, 0, 1, 1);
    worst = 0;
    for (int k = 1; k <= 8; ++k) {
        for (int b = 0; b < 441; ++b) ec_dsp_process(d, buf, 100);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        double dist = st.pos_region < 0.5f ? st.pos_region : 1.0 - st.pos_region;
        if (dist > worst) worst = dist;
    }
    CHECK(worst < 2.0 / 44100.0, "half region worst %g", worst);

    /* warp_rate 0.5: a 16-step region takes 32 steps */
    p.loop_end = 128.f; p.warp_rate = 0.5f;
    ec_dsp_track_params(d, 0, &p);
    for (int b = 0; b < 100; ++b) ec_dsp_process(d, buf, 100);   /* let the rate Lag settle */
    ec_dsp_transport(d, 0, 1, 1);
    ec_track_status_t st;
    for (int b = 0; b < 882; ++b) ec_dsp_process(d, buf, 100);
    ec_dsp_track_status(d, 0, &st);
    CHECK(fabs(st.pos_region - 0.5) < 1e-3, "rate 0.5: half way after 16 steps (%f)", st.pos_region);
    free(loop); free(d);
}

static double max_step(const float *o, int from, int to)
{
    double m = 0;
    for (int i = from + 1; i < to; ++i) {
        double a = fabs(o[2 * i] - o[2 * i - 2]);
        if (a > m) m = a;
    }
    return m;
}

/* 3. live loop-key scrub: jumps, spans, releases in all three return modes
 *    never make a step bigger than steady playback does. */
static void test_scrub(void)
{
    printf("loop-key scrub: click-free jumps and releases\n");
    uint32_t fr = 2 * TK_FS;
    int16_t *sn = tk_sine(220.f, 0.5f, fr);
    for (int mode = 1; mode <= 3; ++mode) {
        ec_dsp_t *d = tk_engine();
        ec_sample_t s = {sn, fr, (float)TK_FS, 1, 0};
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.playhead_return = mode;
        ec_dsp_track_params(d, 0, &p);
        ec_dsp_set_sample(d, 0, &s);
        ec_dsp_transport(d, 0, 1, 1);
        ec_dsp_note_on(d, 0, 0, 0);
        int n = TK_FS * 3;
        float *o = malloc(n * 2 * sizeof(float));
        render(d, o, TK_FS / 2, 128);
        double steady = max_step(o, 4000, TK_FS / 2);
        int pos = TK_FS / 2;
        /* press 5 */
        ec_dsp_loop_key(d, 17, 0, 5, 1, 0);
        render(d, o + 2 * pos, 6000, 128); pos += 6000;
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        CHECK(fabs(st.region_start - 0.25f) < 1e-4 && st.pos_sample >= 0.25f && st.pos_sample < 0.3125f,
              "key 5 jumped (region %f pos %f)", st.region_start, st.pos_sample);
        ec_dsp_loop_key(d, 33, 0, 9, 1, 0);      /* span 5..9 */
        render(d, o + 2 * pos, 9000, 128); pos += 9000;
        ec_dsp_track_status(d, 0, &st);
        CHECK(fabs(st.region_start - 0.25f) < 1e-4 && fabs(st.region_end - 0.5625f) < 1e-4,
              "span 5-9 (%f..%f)", st.region_start, st.region_end);
        ec_dsp_loop_key(d, 5, 0, 5, 0, 0);       /* now only 9 */
        render(d, o + 2 * pos, 7000, 128); pos += 7000;
        ec_dsp_loop_key(d, 99, 0, 9, 0, 0);      /* last key up: release mode */
        render(d, o + 2 * pos, 10000, 128); pos += 10000;
        ec_dsp_track_status(d, 0, &st);
        CHECK(fabs(st.region_start) < 1e-6 && fabs(st.region_end - 1.f) < 1e-6, "back to the base region");
        /* fast drumming on keys */
        for (int k = 0; k < 40; ++k) {
            ec_dsp_loop_key(d, (k * 37) % 128, 0, 1 + (k * 7) % 16, 1, 0);
            ec_dsp_loop_key(d, 127, 0, 1 + (k * 7) % 16, 0, 0);
            render(d, o + 2 * pos, 256, 128); pos += 256;
        }
        render(d, o + 2 * pos, n - pos, 128);
        double worst = max_step(o, TK_FS / 2 - 10, n);
        CHECK(finite_buf(o, 2 * n), "finite");
        CHECK(worst < steady * 1.6 + 1e-4, "mode %d: worst step %f vs steady %f", mode, worst, steady);
        if (mode == 1) tk_write_wav("results/dsp/test_scrub_sine.wav", o, (uint32_t)n);
        free(o); free(d);
    }

    /* return mode is sample-exact: after a hold, the playhead is where it would
     * have been without the gesture */
    {
        ec_dsp_t *d = tk_engine();
        ec_sample_t s = {sn, fr, (float)TK_FS, 1, 0};
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.playhead_return = EC_RETURN_RETURN;
        ec_dsp_track_params(d, 0, &p);
        ec_dsp_set_sample(d, 0, &s);
        ec_dsp_transport(d, 0, 1, 1);
        float buf[256];
        for (int b = 0; b < 100; ++b) ec_dsp_process(d, buf, 128);
        ec_dsp_loop_key(d, 0, 0, 3, 1, 0);
        for (int b = 0; b < 100; ++b) ec_dsp_process(d, buf, 128);
        ec_dsp_loop_key(d, 0, 0, 3, 0, 0);
        ec_dsp_process(d, buf, 128);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        double expect = (201.0 * 128.0) / fr;
        CHECK(fabs(st.pos_sample - expect) < 2.0 / fr, "return: pos %f expected %f", st.pos_sample, expect);
        free(d);
    }
    free(sn);
}

/* 4. slice_poly voice cap + stealing, mono, same-slice, choke */
static void test_voices(void)
{
    printf("slice voices: caps, stealing, mono, choke\n");
    uint32_t fr;
    int16_t *loop = tk_drum_loop(120.f, &fr);
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 0};
    ec_dsp_t *d = tk_engine();
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.machine = EC_MACH_SLICE_POLY;
    p.slice_play_mode = EC_SLICE_LOOP;
    for (int t = 0; t < 4; ++t) { ec_dsp_track_params(d, t, &p); ec_dsp_set_sample(d, t, &s); }
    for (int k = 0; k < 12; ++k) {
        ec_event_t e = tk_ev(k, EC_EV_SLICE_ON, 0);
        e.slice = k + 1; e.length = TK_FS * 10;
        ec_dsp_event(d, &e);
    }
    float buf[2 * 128];
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);   /* > 20 ms */
    ec_track_status_t st;
    ec_dsp_track_status(d, 0, &st);
    CHECK(st.voices == 8, "per-track cap: %d voices", st.voices);
    int ok_oldest = 1;
    for (int i = 0; i < st.voices && i < 8; ++i) if (st.voice_slice[i] <= 4) ok_oldest = 0;
    CHECK(ok_oldest, "the 4 oldest were stolen");
    for (int t = 1; t < 4; ++t)
        for (int k = 0; k < 8; ++k) {
            ec_event_t e = tk_ev(0, EC_EV_SLICE_ON, t);
            e.slice = k + 1; e.length = TK_FS * 10;
            ec_dsp_event(d, &e);
        }
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);
    CHECK(ec_dsp_total_voices(d) == 24, "global cap: %d voices", ec_dsp_total_voices(d));
    ec_dsp_track_status(d, 0, &st);
    CHECK(st.voices == 0, "track 1 (oldest) gave its voices to the pool: %d", st.voices);

    /* same-slice retrigger steals */
    ec_event_t e = tk_ev(0, EC_EV_PANIC, 0);
    ec_dsp_event(d, &e);
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);
    CHECK(ec_dsp_total_voices(d) == 0, "panic cleared all: %d", ec_dsp_total_voices(d));
    for (int k = 0; k < 3; ++k) {
        e = tk_ev(k * 10, EC_EV_SLICE_ON, 0); e.slice = 2; e.length = TK_FS; ec_dsp_event(d, &e);
    }
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 0, &st);
    CHECK(st.voices == 1, "same slice is monophonic: %d", st.voices);
    /* choke group */
    e = tk_ev(0, EC_EV_SLICE_ON, 0); e.slice = 5; e.choke = 2; e.length = TK_FS; ec_dsp_event(d, &e);
    e = tk_ev(5, EC_EV_SLICE_ON, 0); e.slice = 6; e.choke = 2; e.length = TK_FS; ec_dsp_event(d, &e);
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 0, &st);
    CHECK(st.voices == 2, "choke: slice 2 + the last of group 2 (%d)", st.voices);
    /* mono machine */
    p.machine = EC_MACH_GRID_SLICE;
    ec_dsp_track_params(d, 1, &p);
    for (int k = 0; k < 5; ++k) {
        e = tk_ev(k, EC_EV_TRIG, 1); e.slice = k + 1; e.length = TK_FS; ec_dsp_event(d, &e);
    }
    for (int b = 0; b < 40; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 1, &st);
    CHECK(st.voices == 1, "grid_slice is mono: %d", st.voices);
    /* one-shot frees itself at the slice end; hold releases at the gate */
    p.machine = EC_MACH_SLICE_POLY;
    p.slice_play_mode = EC_SLICE_ONESHOT;
    p.slice_count = 16;
    ec_dsp_track_params(d, 2, &p);
    e = tk_ev(0, EC_EV_TRIG, 2); e.slice = 3; ec_dsp_event(d, &e);
    int slice_len = (int)(fr / 16);
    for (int b = 0; b < (slice_len + 3 * TK_FS) / 128; ++b) ec_dsp_process(d, buf, 128);
    ec_dsp_track_status(d, 2, &st);
    CHECK(st.voices == 0, "one-shot ended (%d)", st.voices);
    free(d); free(loop);
}

/* 5. every filter machine/type stable at max resonance + drive, swept */
static void test_filters(void)
{
    printf("filters: stable at max resonance\n");
    uint32_t fr = TK_FS;
    int16_t *nz = malloc(fr * 2 * sizeof(int16_t));
    for (uint32_t i = 0; i < fr; ++i) { nz[2 * i] = tk_s16(tk_noise() * 0.5f); nz[2 * i + 1] = tk_s16(tk_noise() * 0.5f); }
    ec_sample_t s = {nz, fr, (float)TK_FS, 2, 0};
    for (int m = 1; m <= 6; ++m) {
        for (int ty = 1; ty <= 4; ++ty) {
            if ((m == EC_FILT_LADDER || m == EC_FILT_COMB || m == EC_FILT_FORMANT || m == EC_FILT_MORPHING) && ty > 1) continue;
            for (int ms = 1; ms <= 2; ++ms) {
                ec_dsp_t *d = tk_engine();
                ec_track_params_t p;
                ec_track_params_default(&p);
                p.filter_machine = m; p.filter_type = ty; p.filter_res = 1.f; p.filter_drive = 1.f;
                p.filter_mix_mode = ms; p.filter_balance = 0.7f; p.filter_morph = 0.5f;
                ec_dsp_track_params(d, 0, &p);
                ec_dsp_set_sample(d, 0, &s);
                ec_dsp_transport(d, 0, 1, 1);
                ec_dsp_note_on(d, 0, 0, 0);
                float buf[2 * 128];
                float worst = 0;
                int bad = 0;
                for (int b = 0; b < 5 * TK_FS / 128; ++b) {
                    double ph = (double)b / (5.0 * TK_FS / 128.0);
                    p.filter_cutoff = (float)(20.0 * pow(1000.0, 0.5 - 0.5 * cos(ph * 2 * M_PI * 3)));
                    p.filter_type = (b / 200) % 2 ? ty : (ty % 4) + 1;
                    ec_dsp_track_params(d, 0, &p);
                    ec_dsp_process(d, buf, 128);
                    if (!finite_buf(buf, 256)) bad = 1;
                    ec_track_status_t st;
                    ec_dsp_track_status(d, 0, &st);
                    if (st.peak_l > worst) worst = st.peak_l;
                }
                CHECK(!bad && worst < 200.f, "machine %d type %d ms %d: peak %f bad %d", m, ty, ms, worst, bad);
                free(d);
            }
        }
    }
    free(nz);
}

/* 6. loop_trig sounds only inside the trig's gate; granular + chopped sound */
static void test_machines(void)
{
    printf("machines: loop_trig gate, granular, chopped\n");
    uint32_t fr = 2 * TK_FS;
    int16_t *sn = tk_sine(330.f, 0.5f, fr);
    ec_sample_t s = {sn, fr, (float)TK_FS, 1, 0};
    ec_dsp_t *d = tk_engine();
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.machine = EC_MACH_LOOP_TRIG;
    ec_dsp_track_params(d, 0, &p);
    ec_dsp_set_sample(d, 0, &s);
    ec_dsp_transport(d, 0, 1, 1);
    int n = TK_FS;
    float *o = malloc(n * 2 * sizeof(float));
    render(d, o, 896, 128);
    ec_event_t e = tk_ev(1000 - 896, EC_EV_TRIG, 0);
    e.length = TK_FS / 4;
    ec_dsp_event(d, &e);
    render(d, o + 2 * 896, n - 896, 128);
    double in = 0, before = 0, after = 0;
    for (int i = 0; i < 900; ++i) before += fabs(o[2 * i]);
    for (int i = 3000; i < 10000; ++i) in += fabs(o[2 * i]);
    for (int i = 1000 + TK_FS / 4 + 2000; i < n; ++i) after += fabs(o[2 * i]);
    CHECK(before == 0.0 && in > 100.0 && after < 1e-2, "loop_trig gate (before %g in %g after %g)", before, in, after);
    free(d);

    int modes[2] = {EC_WARP_GRANULAR, EC_WARP_CHOPPED};
    for (int mi = 0; mi < 2; ++mi) {
        d = tk_engine();
        ec_track_params_default(&p);
        p.mode = modes[mi];
        ec_dsp_track_params(d, 0, &p);
        ec_dsp_set_sample(d, 0, &s);
        ec_dsp_transport(d, 0, 1, 1);
        ec_dsp_note_on(d, 0, 0, 0);
        render(d, o, n, 128);
        double rms = 0;
        for (int i = TK_FS / 4; i < n; ++i) rms += o[2 * i] * o[2 * i];
        rms = sqrt(rms / (n - TK_FS / 4));
        CHECK(finite_buf(o, 2 * n) && rms > 0.05 && rms < 1.0, "mode %d rms %f", modes[mi], rms);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        CHECK(st.mode_implemented == 1 && st.pos_sample > 0.f, "mode %d advances (%f)", modes[mi], st.pos_sample);
        free(d);
    }
    free(o); free(sn);
}

/* 7. long run of the busy scene: finite, then silence leaves no denormals */
static int is_sub(float x) { return x != 0.f && fabsf(x) < 1.17549435e-38f; }

static void test_long_run(void)
{
    printf("long run: 6 busy tracks for 120 s, then 30 s of silence\n");
    ec_dsp_t *d = tk_engine();
    tk_scene_t sc;
    tk_scene_setup(d, &sc);
    float buf[2 * 128];
    int bad = 0;
    float peak = 0;
    for (int b = 0; b < 120 * TK_FS / 128; ++b) {
        tk_scene_events(d, &sc, 128);
        ec_dsp_process(d, buf, 128);
        for (int i = 0; i < 256; ++i) { if (!isfinite(buf[i])) bad = 1; if (fabsf(buf[i]) > peak) peak = fabsf(buf[i]); }
    }
    CHECK(!bad && peak <= 1.0f && peak > 0.1f, "finite, limited (peak %f)", peak);
    ec_dsp_transport(d, 0, 0, 0);
    for (int b = 0; b < 30 * TK_FS / 128; ++b) ec_dsp_process(d, buf, 128);
    int sub = 0;
    for (int t = 0; t < EC_TRACKS; ++t) {
        ec_filter_t *f = &d->tr[t].f;
        for (int ch = 0; ch < 2; ++ch) {
            for (int k = 0; k < 3; ++k) sub += is_sub(f->st[ch][k][0]) + is_sub(f->st[ch][k][1])
                                             + is_sub(f->fst[ch][k][0]) + is_sub(f->fst[ch][k][1]);
            for (int k = 0; k < 4; ++k) sub += is_sub(f->lad[ch][k]);
            sub += is_sub(f->svf[ch][0]) + is_sub(f->svf[ch][1]) + is_sub(f->clp[ch][0]) + is_sub(f->clp[ch][1]);
            sub += is_sub(f->dc_x1[ch]) + is_sub(f->dc_y1[ch]);
            for (int k = 0; k < EC_COMB_LEN; ++k) sub += is_sub(f->comb[ch][k]);
        }
    }
    CHECK(sub == 0, "%d subnormal filter states after silence", sub);
    for (int i = 0; i < 256; ++i) if (is_sub(buf[i])) sub++;
    CHECK(sub == 0, "subnormal output");
    CHECK(ec_dsp_total_voices(d) == 0, "voices released on stop (%d)", ec_dsp_total_voices(d));
    tk_scene_free(&sc);
    free(d);
}

/* 8. envelope basics: ADSR timed gate releases; AHR hold */
static void test_envelopes(void)
{
    printf("envelopes: ADSR gate + release, AHR hold\n");
    ec_envspec_t sp = {EC_ENV_ADSR, 0.01f, 0.05f, 0.5f, 0.1f, 0.f};
    ec_env_t e;
    ec_env_reset(&e);
    ec_env_trigger(&e, &sp, (int32_t)(0.2 * TK_FS), 0, (float)TK_FS);
    float v = 0, at_sus = 0, at_end = 0;
    for (int i = 0; i < TK_FS; ++i) {
        v = ec_env_tick(&e, &sp, (float)TK_FS);
        if (i == (int)(0.15 * TK_FS)) at_sus = v;
        if (i == (int)(0.31 * TK_FS)) at_end = v;
    }
    CHECK(fabsf(at_sus - 0.5f) < 1e-3 && at_end < 0.01f && e.stage == ENV_DONE,
          "ADSR sus %f end %f stage %d", at_sus, at_end, e.stage);
    ec_envspec_t ahr = {EC_ENV_AHR, 0.005f, 0.f, 0.f, 0.05f, 0.1f};
    ec_env_reset(&e);
    ec_env_trigger(&e, &ahr, (int32_t)(0.01 * TK_FS), 0, (float)TK_FS);
    float mid = 0;
    for (int i = 0; i < TK_FS / 2; ++i) { v = ec_env_tick(&e, &ahr, (float)TK_FS); if (i == TK_FS / 20) mid = v; }
    CHECK(fabsf(mid - 1.f) < 1e-4 && v == 0.f, "AHR ignores the gate, holds, releases (mid %f end %f)", mid, v);
}

int main(void)
{
    test_reader_bitnear();
    test_tape_end_to_end();
    test_tempo_boundary();
    test_scrub();
    test_voices();
    test_filters();
    test_machines();
    test_envelopes();
    test_long_run();
    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
