/*
 * test_e3.c -- E3 engine tests: the real FFT, every new warp mode makes bounded
 * non-silent sound, the pitch-holding modes hold pitch, the harmonizer adds its
 * interval, spectral freeze holds, the neighbor machine reroutes the left track.
 * Exit code 0 = all passed. With an argument (a folder), writes a WAV per mode.
 */
#include "ec_testkit.h"
#include "../../src/dsp/ec_internal.h"

static int g_fail = 0, g_pass = 0;
#define CHECK(cond, ...) do { if (cond) { g_pass++; } else { g_fail++; \
    printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *g_wavdir = NULL;

static void render(ec_dsp_t *d, float *out, int frames)
{
    for (int pos = 0; pos < frames; pos += 128) {
        int n = frames - pos < 128 ? frames - pos : 128;
        ec_dsp_process(d, out + 2 * pos, n);
    }
}

static double rms(const float *b, int from, int to)
{
    double s = 0;
    for (int i = from; i < to; ++i) s += 0.5 * (b[2 * i] * b[2 * i] + b[2 * i + 1] * b[2 * i + 1]);
    return sqrt(s / (to - from > 0 ? to - from : 1));
}
static double peak(const float *b, int from, int to)
{
    double p = 0;
    for (int i = 2 * from; i < 2 * to; ++i) { if (!isfinite(b[i])) return 1e9; if (fabs(b[i]) > p) p = fabs(b[i]); }
    return p;
}
/* |DFT| of the left channel at f Hz (Goertzel), normalised */
static double tone(const float *b, int from, int to, double f)
{
    double w = 2 * M_PI * f / TK_FS, c = 2 * cos(w), s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i) { double s0 = b[2 * i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return sqrt(s1 * s1 + s2 * s2 - c * s1 * s2) / (to - from);
}
/* energy in [f - bw, f + bw] (1 Hz steps) */
static double band(const float *b, int from, int to, double f, double bw)
{
    double e = 0;
    for (double x = f - bw; x <= f + bw; x += 1.0) { double m = tone(b, from, to, x); e += m * m; }
    return e;
}
/* a band-limited saw (20 harmonics), int16 stereo: a pitch with many partials (a granular pitch shifter
 * comb-filters a pure sine between its grains, as SC's PitchShift does, so sines make poor test tones) */
static int16_t *saw(double f, float amp, uint32_t frames)
{
    int16_t *d = malloc(frames * 2 * sizeof(int16_t));
    for (uint32_t i = 0; i < frames; ++i) {
        double v = 0;
        for (int k = 1; k <= 20; ++k) v += sin(2 * M_PI * f * k * i / TK_FS) / k;
        int16_t x = tk_s16(amp * (float)v * 0.55f);
        d[2 * i] = x; d[2 * i + 1] = x;
    }
    return d;
}
/* the fundamental (Hz) from the normalised autocorrelation of the left channel: the first lag in
 * [fs/hi, fs/lo] whose correlation is within 5 % of the best */
static double pitch_acf(const float *b, int from, int to, double lo, double hi)
{
    int l0 = (int)(TK_FS / hi), l1 = (int)(TK_FS / lo), best = l0;
    static double r[2048];
    double rmax = -1;
    for (int L = l0; L <= l1 && L < 2048; ++L) {
        double xy = 0, xx = 0, yy = 0;
        for (int i = from; i < to - L; ++i) { double x = b[2 * i], y = b[2 * (i + L)]; xy += x * y; xx += x * x; yy += y * y; }
        r[L] = xy / sqrt(xx * yy + 1e-30);
        if (r[L] > rmax) rmax = r[L];
    }
    for (int L = l0; L <= l1 && L < 2048; ++L) if (r[L] >= rmax * 0.95) { best = L; break; }
    /* parabolic refinement */
    double y0 = best > l0 ? r[best - 1] : r[best], y1 = r[best], y2 = best < l1 ? r[best + 1] : r[best];
    double den = y0 - 2 * y1 + y2, off = fabs(den) > 1e-12 ? 0.5 * (y0 - y2) / den : 0;
    return TK_FS / (best + off);
}
/* the strongest frequency between lo and hi (1 Hz steps) */
static double strongest(const float *b, int from, int to, double lo, double hi)
{
    double best = 0, bf = lo;
    for (double f = lo; f <= hi; f += 1.0) { double m = tone(b, from, to, f); if (m > best) { best = m; bf = f; } }
    return bf;
}

/* ------------------------------------------------------------ FFT */
static void test_fft(void)
{
    printf("fft: 2048-point real FFT against a direct DFT, inverse\n");
    static ec_rfft_t f;
    static float x[EC_FFT_N], y[EC_FFT_N];
    ec_rfft_init(&f);
    for (int i = 0; i < EC_FFT_N; ++i) x[i] = y[i] = (float)(sin(i * 0.37) + 0.5 * cos(i * 1.9) + 0.2 * tk_noise());
    ec_rfft_fwd(&f, y);
    double maxe = 0;
    for (int k = 0; k <= EC_FFT_HALF; k += 7) {
        double re = 0, im = 0;
        for (int n = 0; n < EC_FFT_N; ++n) { re += x[n] * cos(-2 * M_PI * k * n / EC_FFT_N); im += x[n] * sin(-2 * M_PI * k * n / EC_FFT_N); }
        double gr = k == 0 ? y[0] : k == EC_FFT_HALF ? y[1] : y[2 * k], gi = (k == 0 || k == EC_FFT_HALF) ? 0 : y[2 * k + 1];
        double e = fabs(gr - re) + fabs(gi - im);
        if (e > maxe) maxe = e;
    }
    CHECK(maxe < 1e-3, "forward error %g", maxe);
    ec_rfft_inv(&f, y);
    double ie = 0;
    for (int i = 0; i < EC_FFT_N; ++i) { double e = fabs(y[i] * 2.0 / EC_FFT_N - x[i]); if (e > ie) ie = e; }
    CHECK(ie < 1e-5, "inverse error %g", ie);
}

/* ------------------------------------------------------------ one loop track in a mode */
typedef struct { int16_t *data; uint32_t frames; ec_sample_t s; } smp_t;

static ec_dsp_t *one_track(const ec_sample_t *s, ec_track_params_t *p, float bpm)
{
    ec_dsp_t *d = tk_engine();
    ec_dsp_track_params(d, 0, p);
    ec_dsp_set_sample(d, 0, s);
    ec_dsp_set_tempo(d, bpm);
    ec_dsp_transport(d, 0, 1, 1);
    ec_dsp_note_on(d, 0, 0, 0);
    return d;
}

static void save(const char *name, const float *o, int n)
{
    char path[512];
    if (!g_wavdir) return;
    snprintf(path, sizeof path, "%s/%s.wav", g_wavdir, name);
    tk_write_wav(path, o, (uint32_t)n);
}

static void test_modes_sound(void)
{
    printf("warp modes: every E3 mode makes bounded, non-silent sound (drum loop, 6 s)\n");
    uint32_t fr;
    int16_t *loop = tk_drum_loop(120.f, &fr);
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 120.f};
    static const int modes[] = {EC_WARP_DOMINO, EC_WARP_RANDOM_OLA, EC_WARP_PITCH_CORRECTED, EC_WARP_HARMONIZER,
                                EC_WARP_WAVETABLE, EC_WARP_SPECTRAL_FREEZE, EC_WARP_FORMANT, EC_WARP_GSTRETCH,
                                EC_WARP_GSTRETCH2};
    static const char *names[] = {"domino", "random_ola", "pitch_corrected", "harmonizer", "wavetable",
                                  "spectral_freeze", "formant", "gstretch", "gstretch2"};
    int n = 6 * TK_FS;
    float *o = malloc((size_t)n * 2 * sizeof(float));
    for (int m = 0; m < 9; ++m) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = modes[m];
        p.sample_steps = 16.f;
        p.mode_macro = 0.5f;
        p.harm_interval[0] = 7.f; p.harm_interval[1] = 12.f; p.harm_interval[2] = -12.f;
        p.wt_lfo_rate = 0.5f; p.wt_lfo_depth = 0.5f;
        p.spectral_blur = 0.3f; p.formant_shift = m == 6 ? 5.f : 0.f;
        p.grain_speed = 0.75f;
        ec_dsp_t *d = one_track(&s, &p, 100.f);
        render(d, o, n);
        double r = rms(o, TK_FS, n), pk = peak(o, 0, n);
        ec_track_status_t st;
        ec_dsp_track_status(d, 0, &st);
        printf("  %-16s rms %.3f peak %.3f\n", names[m], r, pk);
        CHECK(r > 0.02, "%s silent (rms %.4f)", names[m], r);
        CHECK(pk <= 1.0 && pk > 0.05, "%s peak %.3f", names[m], pk);
        CHECK(st.mode_implemented, "%s reports a fallback", names[m]);
        save(names[m], o, n);
        free(d);
    }
    free(o);
    free(loop);
}

/* ------------------------------------------------------------ pitch */
static void test_pitch_holding(void)
{
    printf("pitch: pitch_corrected / gstretch / gstretch2 hold a 220 Hz buzz while the read speed changes\n");
    uint32_t fr = 2 * TK_FS;                    /* 2 s = 16 steps at 120 BPM */
    int16_t *sn = saw(220.0, 0.5f, fr);
    ec_sample_t s = {sn, fr, (float)TK_FS, 1, 120.f};
    int n = 3 * TK_FS;
    float *o = malloc((size_t)n * 2 * sizeof(float));
    struct { int mode; float bpm, strch; const char *name; } C[] = {
        {EC_WARP_TEMPO_VARISPEED, 90.f, 1.f, "tempo_varispeed (control: 165 Hz)"},
        {EC_WARP_PITCH_CORRECTED, 90.f, 1.f, "pitch_corrected at 90 BPM"},
        {EC_WARP_GSTRETCH, 120.f, 0.5f, "gstretch STRCH 0.5"},
        {EC_WARP_GSTRETCH2, 90.f, 1.f, "gstretch2 at 90 BPM"},
    };
    for (int c = 0; c < 4; ++c) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = C[c].mode;
        p.sample_steps = 16.f;
        p.grain_speed = C[c].strch;
        ec_dsp_t *d = one_track(&s, &p, C[c].bpm);
        render(d, o, n);
        double want = c == 0 ? 165.0 : 220.0, f = pitch_acf(o, TK_FS, TK_FS + TK_FS / 2, 120.0, 400.0);
        printf("  %-36s %.1f Hz\n", C[c].name, f);
        CHECK(fabs(f - want) < want * 0.02, "%s: %.1f Hz, want %.0f", C[c].name, f, want);
        free(d);
    }
    free(o);
    free(sn);
}

static void test_harmonizer(void)
{
    printf("harmonizer: IN1 +7 at MIX 1 adds the fifth (300 Hz over a 200 Hz buzz); MIX 0 = dry only\n");
    uint32_t fr = 2 * TK_FS;
    int16_t *sn = saw(200.0, 0.4f, fr);
    ec_sample_t s = {sn, fr, (float)TK_FS, 1, 120.f};
    int n = 3 * TK_FS;
    float *o = malloc((size_t)n * 2 * sizeof(float));
    double oct[2];
    for (int k = 0; k < 2; ++k) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = EC_WARP_HARMONIZER;
        p.sample_steps = 16.f;
        p.mode_macro = k ? 1.f : 0.f;
        p.harm_interval[0] = 7.f;
        ec_dsp_t *d = one_track(&s, &p, 120.f);
        render(d, o, n);
        oct[k] = band(o, TK_FS, n, 299.7, 8) / (band(o, TK_FS, n, 200.0, 8) + 1e-12);
        free(d);
    }
    printf("  300 / 200 Hz band energy: MIX 0 %.4f, MIX 1 %.3f\n", oct[0], oct[1]);
    CHECK(oct[0] < 0.001, "MIX 0 has an octave (%.4f)", oct[0]);
    CHECK(oct[1] > 0.1, "MIX 1 fifth too weak (%.3f)", oct[1]);
    free(o);
    free(sn);
}

static void test_spectral(void)
{
    printf("spectral: pitch +12 moves the bins up an octave; FREEZE holds the spectrum over silence\n");
    uint32_t fr = 2 * TK_FS;
    int16_t *sn = tk_sine(440.f, 0.4f, fr);
    ec_sample_t s = {sn, fr, (float)TK_FS, 1, 120.f};
    int n = 3 * TK_FS;
    float *o = malloc((size_t)n * 2 * sizeof(float));
    {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = EC_WARP_SPECTRAL_FREEZE;
        p.sample_steps = 16.f;
        p.pitch = 12.f;
        ec_dsp_t *d = one_track(&s, &p, 120.f);
        render(d, o, n);
        double f = strongest(o, TK_FS, n, 300, 1200);
        printf("  pitch +12: %.0f Hz\n", f);
        CHECK(fabs(f - 880.0) < 20.0, "BinShift pitch: %.0f Hz", f);
        free(d);
    }
    /* a sample: 1 s tone then 1 s silence; FREEZE engages in the tone and must keep sounding in the silence */
    for (int i = (int)TK_FS; i < (int)fr; ++i) sn[2 * i] = sn[2 * i + 1] = 0;
    double held[2];
    for (int k = 0; k < 2; ++k) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = EC_WARP_SPECTRAL_FREEZE;
        p.sample_steps = 16.f;
        ec_dsp_t *d = one_track(&s, &p, 120.f);
        render(d, o, TK_FS / 2);
        p.freeze_amount = k ? 1.f : 0.f;
        ec_dsp_track_params(d, 0, &p);
        render(d, o, TK_FS + TK_FS / 2);          /* now in the silent half (+ FFT latency) */
        held[k] = rms(o, TK_FS + TK_FS / 10, TK_FS + TK_FS / 2);
        free(d);
    }
    printf("  rms in the silence: FRZE 0 %.4f, FRZE 1 %.4f\n", held[0], held[1]);
    CHECK(held[0] < 0.002, "unfrozen not silent (%.4f)", held[0]);
    CHECK(held[1] > 0.02, "frozen spectrum did not hold (%.4f)", held[1]);
    free(o);
    free(sn);
}

/* ------------------------------------------------------------ the spectral budget hook */
static void test_budget(void)
{
    printf("spectral budget: tracks past EC_W3_SPECTRAL_TRACKS fall back to tape (%d allowed)\n",
           EC_W3_SPECTRAL_TRACKS);
    ec_dsp_t *d = tk_engine();
    uint32_t fr;
    int16_t *loop = tk_drum_loop(120.f, &fr);
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 120.f};
    for (int t = 0; t < EC_TRACKS; ++t) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.mode = EC_WARP_FORMANT;
        ec_dsp_track_params(d, t, &p);
        ec_dsp_set_sample(d, t, &s);
    }
    float o[256];
    ec_dsp_process(d, o, 128);
    int ok = 1;
    for (int t = 0; t < EC_TRACKS; ++t) {
        ec_track_status_t st;
        ec_dsp_track_status(d, t, &st);
        if (st.mode_implemented != (t < EC_W3_SPECTRAL_TRACKS)) ok = 0;
    }
    CHECK(ok, "budget flags wrong");
    free(d);
    free(loop);
}

/* ------------------------------------------------------------ neighbor */
static void test_neighbor(void)
{
    printf("neighbor: T1 routed into T2 -- silent alone, heard only through T2\n");
    uint32_t fr;
    int16_t *loop = tk_drum_loop(120.f, &fr);
    ec_sample_t s = {loop, fr, (float)TK_FS, 2, 120.f};
    int n = 2 * TK_FS;
    float *o = malloc((size_t)n * 2 * sizeof(float));
    double r[4], hf[4] = {0, 0, 0, 0};
    for (int c = 0; c < 4; ++c) {
        ec_dsp_t *d = tk_engine();
        ec_track_params_t p;
        ec_track_params_default(&p);
        ec_dsp_track_params(d, 0, &p);
        ec_dsp_set_sample(d, 0, &s);
        ec_track_params_default(&p);
        p.machine = c == 0 ? EC_MACH_LOOP : EC_MACH_NEIGHBOR;   /* c 0: T2 an empty loop track */
        if (c == 2) p.mute = 1;                                 /* T2 muted: T1 must vanish too */
        if (c == 3) { p.filter_cutoff = 300.f; }                /* T2's filter shapes T1 */
        ec_dsp_track_params(d, 1, &p);
        ec_dsp_transport(d, 0, 1, 1);
        ec_dsp_note_on(d, 0, 0, 0);
        render(d, o, n);
        r[c] = rms(o, TK_FS / 2, n);
        hf[c] = band(o, TK_FS / 2, n, 5000.0, 400);
        if (c == 1) save("neighbor_t1_through_t2", o, n);
        free(d);
    }
    printf("  rms: T1 alone %.4f, T1 -> T2 %.4f, T2 muted %.5f, T2 LP 300 %.4f\n", r[0], r[1], r[2], r[3]);
    CHECK(r[0] > 0.02, "T1 alone silent");
    CHECK(r[1] > 0.02, "nothing heard through the neighbor");
    CHECK(r[2] < 1e-4, "T1 still reaches the master when T2 is muted (%.5f)", r[2]);
    printf("  4.6-5.4 kHz energy: T1 -> T2 %.3g, through T2's 300 Hz LP %.3g\n", hf[1], hf[3]);
    CHECK(hf[3] < hf[1] * 0.01, "T2's filter does not shape T1 (%.3g vs %.3g)", hf[3], hf[1]);
    free(o);
    free(loop);
}

int main(int argc, char **argv)
{
    (void)tk_ev;
    if (argc > 1) g_wavdir = argv[1];
    test_fft();
    test_modes_sound();
    test_pitch_holding();
    test_harmonizer();
    test_spectral();
    test_budget();
    test_neighbor();
    printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
