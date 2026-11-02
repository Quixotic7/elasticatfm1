/*
 * render_demo.c -- writes listening WAVs of the engine to results/dsp/:
 * every machine, warp mode and filter on a synthesized drum + bass loop.
 * A tiny step clock calls a per-demo callback on each 16th note.
 */
#include "ec_scene.h"

typedef void (*step_fn)(ec_dsp_t *d, int step, int off, double step_len);

static float *g_out;
static uint32_t g_len, g_cap;

static void out_reset(void) { g_len = 0; }
static void out_push(const float *b, int n)
{
    if (g_len + (uint32_t)n > g_cap) {
        g_cap = (g_len + n) * 2;
        g_out = realloc(g_out, g_cap * 2 * sizeof(float));
    }
    memcpy(g_out + 2 * g_len, b, n * 2 * sizeof(float));
    g_len += n;
}

/* run `steps` 16ths at the engine tempo, calling fn at each step start */
static int g_step;
static void run_steps(ec_dsp_t *d, int steps, step_fn fn)
{
    double step_len = 15.0 * TK_FS / ec_dsp_tempo(d);
    double next = 0, clock = 0;
    int done = 0;
    float buf[2 * 128];
    while (done < steps) {
        while (next < clock + 128 && done < steps) {
            if (fn) fn(d, g_step, (int)(next - clock), step_len);
            g_step++; done++;
            next += step_len;
        }
        int n = 128;
        ec_dsp_process(d, buf, n);
        out_push(buf, n);
        clock += n;
    }
    /* finish the last step */
    while (clock < next) { ec_dsp_process(d, buf, 128); out_push(buf, 128); clock += 128; }
}

static void save(const char *name)
{
    char path[256];
    snprintf(path, sizeof path, "results/dsp/%s", name);
    tk_write_wav(path, g_out, g_len);
    printf("  %s (%.1f s)\n", path, g_len / (double)TK_FS);
}

static int16_t *g_l120, *g_l100;
static uint32_t g_f120, g_f100;
static ec_sample_t S120, S100;

static ec_dsp_t *fresh(ec_track_params_t *p, const ec_sample_t *s)
{
    ec_dsp_t *d = tk_engine();
    ec_dsp_track_params(d, 0, p);
    ec_dsp_set_sample(d, 0, s);
    ec_dsp_set_tempo(d, 120.f);
    ec_dsp_transport(d, 0, 1, 1);
    g_step = 0;
    out_reset();
    return d;
}

static void note_each_bar(ec_dsp_t *d, int step, int off, double sl)
{
    (void)sl;
    if (step % 16 == 0) { ec_event_t e = tk_ev(off, EC_EV_TRIG, 0); ec_dsp_event(d, &e); }
}
static void note_each_beat(ec_dsp_t *d, int step, int off, double sl)
{
    if (step % 4 == 0) { ec_event_t e = tk_ev(off, EC_EV_TRIG, 0); e.length = (int)(sl * 3); ec_dsp_event(d, &e); }
}

/* ---- demos ------------------------------------------------------------ */
static void demo_tape(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 32, note_each_bar);
    p.pitch = -5.f; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    p.pitch = 7.f; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    p.pitch = 0.f; p.loop_reverse = 1; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    p.loop_reverse = 0; p.warp_rate = 0.5f; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    save("02_tape_pitch_reverse_rate.wav");
    free(d);
}

static void demo_tempo(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.mode = EC_WARP_TEMPO_VARISPEED;
    ec_dsp_t *d = fresh(&p, &S100);   /* a 100 BPM loop, fitted to 120 */
    run_steps(d, 32, note_each_bar);
    ec_dsp_set_tempo(d, 90.f); run_steps(d, 32, note_each_bar);
    ec_dsp_set_tempo(d, 140.f); run_steps(d, 32, note_each_bar);
    ec_dsp_set_tempo(d, 120.f);
    p.loop_start = 32.f; p.loop_end = 64.f;   /* 4-step region, still on the grid */
    ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    save("03_tempo_varispeed_100bpm_loop_at_120_90_140.wav");
    free(d);
}

static int g_keymode;
static void scrub_steps(ec_dsp_t *d, int step, int off, double sl)
{
    (void)sl;
    if (step % 16 == 0) { ec_event_t e = tk_ev(off, EC_EV_TRIG, 0); ec_dsp_event(d, &e); }
    int s = step % 64;
    /* bar 2: hold key 5 for a beat, then key 13; bar 3: span 9..12; bar 4: stutter */
    if (s == 16) ec_dsp_loop_key(d, off, 0, 5, 1, 0);
    if (s == 20) ec_dsp_loop_key(d, off, 0, 5, 0, 0);
    if (s == 24) ec_dsp_loop_key(d, off, 0, 13, 1, 0);
    if (s == 30) ec_dsp_loop_key(d, off, 0, 13, 0, 0);
    if (s == 32) ec_dsp_loop_key(d, off, 0, 9, 1, 0);
    if (s == 34) ec_dsp_loop_key(d, off, 0, 12, 1, 0);
    if (s == 40) { ec_dsp_loop_key(d, off, 0, 9, 0, 0); }
    if (s == 44) { ec_dsp_loop_key(d, off, 0, 12, 0, 0); }
    if (s >= 48 && s < 60) {
        int k = 1 + ((s - 48) * 5) % 16;
        ec_dsp_loop_key(d, off, 0, k, 1, 0);
        ec_dsp_loop_key(d, off + (int)(sl * 0.75), 0, k, 0, 0);
    }
}
static void demo_scrub(void)
{
    static const char *names[3] = {"return", "boomerang", "reset"};
    for (g_keymode = 1; g_keymode <= 3; ++g_keymode) {
        ec_track_params_t p;
        ec_track_params_default(&p);
        p.playhead_return = g_keymode;
        ec_dsp_t *d = fresh(&p, &S120);
        run_steps(d, 64, scrub_steps);
        char name[96];
        snprintf(name, sizeof name, "04_loopkey_scrub_%s.wav", names[g_keymode - 1]);
        save(name);
        free(d);
    }
}

static void demo_granular(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.mode = EC_WARP_GRANULAR;
    p.amp = 0.45f;
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 32, note_each_bar);
    p.grain_speed = 0.25f; p.grain_jitter = 0.03f; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    p.grain_speed = 0.f; p.pitch = 12.f; p.grain_size = 0.2f; p.grain_density = 16.f;
    ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    p.grain_speed = 1.f; p.pitch = -7.f; p.grain_size = 0.03f; p.grain_direction = 0.5f; p.grain_speed_rand = 0.3f;
    ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    save("05_granular_speed_freeze_pitch.wav");
    free(d);
}

static void demo_chopped(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.mode = EC_WARP_CHOPPED;
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 32, note_each_bar);
    ec_dsp_set_tempo(d, 90.f); run_steps(d, 32, note_each_bar);
    p.chop_slice_len = 0.5f; p.chop_hold = 0.5f; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, note_each_bar);
    ec_dsp_set_tempo(d, 140.f); p.chop_slice_len = 2.f; p.chop_hold = 1.f; ec_dsp_track_params(d, 0, &p);
    run_steps(d, 32, note_each_bar);
    save("06_chopped_120_90_140.wav");
    free(d);
}

static void lt_steps(ec_dsp_t *d, int step, int off, double sl)
{
    static const int pat[16] = {1,0,0,1, 0,0,1,0, 1,0,1,0, 0,1,0,0};
    static const float starts[16] = {0,0,0,64, 0,0,32,0, 96,0,16,0, 0,112,0,0};
    int s = step % 16;
    if (!pat[s]) return;
    ec_event_t e = tk_ev(off, EC_EV_TRIG, 0);
    e.length = (int)(sl * (s == 0 ? 2.5 : 1.5));
    if (starts[s] > 0) { e.start = starts[s]; e.end = starts[s] + 16.f; }
    ec_dsp_event(d, &e);
}
static void demo_loop_trig(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.machine = EC_MACH_LOOP_TRIG;
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 64, lt_steps);
    save("07_loop_trig_gates_and_region_locks.wav");
    free(d);
}

static void slice_steps(ec_dsp_t *d, int step, int off, double sl)
{
    static const int pat[16] = {1, 3, 9, 3, 5, 0, 1, 11, 1, 3, 9, 15, 5, 13, 7, 16};
    int s = step % 16;
    if (!pat[s]) return;
    ec_event_t e = tk_ev(off, EC_EV_TRIG, 0);
    e.slice = pat[s];
    e.length = (int)(sl * 0.9);
    e.velocity = (s % 4 == 0) ? 1.f : 0.7f;
    ec_dsp_event(d, &e);
}
static void demo_grid_slice(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.machine = EC_MACH_GRID_SLICE;
    p.env_release = 0.05f;
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 32, slice_steps);
    p.slice_reverse = 1; ec_dsp_track_params(d, 0, &p); run_steps(d, 32, slice_steps);
    p.slice_reverse = 0; p.slice_play_mode = EC_SLICE_PINGPONG; ec_dsp_track_params(d, 0, &p);
    run_steps(d, 32, slice_steps);
    p.slice_play_mode = EC_SLICE_ONESHOT; p.mode = EC_WARP_TEMPO_VARISPEED; ec_dsp_track_params(d, 0, &p);
    ec_dsp_set_tempo(d, 100.f); run_steps(d, 32, slice_steps);
    save("08_grid_slice_oneshot_reverse_pingpong_tempo.wav");
    free(d);
}

static void poly_steps(ec_dsp_t *d, int step, int off, double sl)
{
    static const float chord[4][3] = {{0, 4, 7}, {-3, 0, 4}, {-7, -3, 0}, {-5, -1, 2}};
    int s = step % 16;
    if (s % 4 == 0) {
        int c = (step / 4) % 4;
        for (int k = 0; k < 3; ++k) {
            ec_event_t e = tk_ev(off, EC_EV_TRIG, 0);
            e.slice = 13 + k;   /* bass region slices */
            e.pitch = chord[c][k];
            e.length = (int)(sl * 3.5);
            e.velocity = 0.6f;
            ec_dsp_event(d, &e);
        }
    }
    if (s % 2 == 1) {
        ec_event_t e = tk_ev(off, EC_EV_TRIG, 0);
        e.slice = 3; e.pitch = 12.f; e.length = (int)(sl * 0.5); e.velocity = 0.4f; e.choke = 1;
        ec_dsp_event(d, &e);
    }
}
static void demo_slice_poly(void)
{
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.machine = EC_MACH_SLICE_POLY;
    p.slice_play_mode = EC_SLICE_LOOP;
    p.env_mode = EC_ENV_ADSR; p.env_attack = 0.01f; p.env_decay = 0.2f; p.env_sustain = 0.7f; p.env_release = 0.3f;
    p.filter_cutoff = 4000.f;
    p.amp = 1.f;
    ec_dsp_t *d = fresh(&p, &S120);
    run_steps(d, 64, poly_steps);
    p.mode = EC_WARP_GRANULAR; p.grain_density = 6.f; p.grain_size = 0.06f;
    ec_dsp_track_params(d, 0, &p);
    run_steps(d, 32, poly_steps);
    save("09_slice_poly_pitched_chords_raw_then_granular.wav");
    free(d);
}

static void demo_filters(void)
{
    static const struct { int mach, type; float res, drive; const char *name; } F[] = {
        {EC_FILT_CLASSIC, EC_FTYPE_LP, 0.8f, 0.f, "classic LP"},
        {EC_FILT_CLASSIC, EC_FTYPE_HP, 0.6f, 0.f, "classic HP"},
        {EC_FILT_CLASSIC, EC_FTYPE_BP, 0.5f, 0.f, "classic BP"},
        {EC_FILT_LADDER, EC_FTYPE_LP, 0.85f, 0.4f, "ladder"},
        {EC_FILT_SVF, EC_FTYPE_LP, 0.9f, 0.f, "svf LP"},
        {EC_FILT_SVF, EC_FTYPE_NOTCH, 0.5f, 0.f, "svf notch"},
        {EC_FILT_MORPHING, EC_FTYPE_LP, 0.6f, 0.f, "morphing"},
        {EC_FILT_COMB, EC_FTYPE_LP, 0.7f, 0.3f, "comb"},
        {EC_FILT_FORMANT, EC_FTYPE_LP, 0.6f, 0.f, "formant"},
    };
    ec_track_params_t p;
    ec_track_params_default(&p);
    p.amp = 0.45f;
    ec_dsp_t *d = fresh(&p, &S120);
    float buf[2 * 128];
    double step_len = 15.0 * TK_FS / 120.0;
    int nf = (int)(sizeof F / sizeof F[0]);
    for (int fi = 0; fi < nf; ++fi) {
        p.filter_machine = F[fi].mach; p.filter_type = F[fi].type;
        p.filter_res = F[fi].res; p.filter_drive = F[fi].drive;
        p.filter_balance = 0.f; p.filter_mix = 1.f;
        int blocks = (int)(32 * step_len / 128);
        for (int b = 0; b < blocks; ++b) {
            double x = (double)b / blocks;
            float v = (float)(10 + 110 * (0.5 - 0.5 * cos(x * 2 * M_PI)));
            p.filter_cutoff = ec_xf_cutoff_hz(v);
            p.filter_morph = (float)x;
            if (F[fi].mach == EC_FILT_COMB) p.filter_cutoff = ec_xf_cutoff_hz(40 + 40 * (float)x);
            p.filter_balance = x > 0.5 ? (float)((x - 0.5) * 2.0) : 0.f;   /* MSBL in the 2nd half */
            ec_dsp_track_params(d, 0, &p);
            if (b % (int)(16 * step_len / 128) == 0) { ec_event_t e = tk_ev(0, EC_EV_TRIG, 0); ec_dsp_event(d, &e); }
            ec_dsp_process(d, buf, 128);
            out_push(buf, 128);
        }
    }
    /* filter envelope depth plucks */
    p.filter_machine = EC_FILT_CLASSIC; p.filter_type = EC_FTYPE_LP; p.filter_res = 0.7f; p.filter_cutoff = 150.f;
    p.filter_env_mode = EC_ENV_ADSR; p.filter_env_attack = 0.001f; p.filter_env_decay = 0.12f;
    p.filter_env_sustain = 0.f; p.filter_env_release = 0.1f; p.filter_env_depth = 0.75f; p.filter_balance = 0.f;
    ec_dsp_track_params(d, 0, &p);
    g_step = 0;
    run_steps(d, 32, note_each_beat);
    save("10_filters_classic_ladder_svf_morph_comb_formant_env.wav");
    free(d);
}

static void demo_mix(void)
{
    ec_dsp_t *d = tk_engine();
    tk_scene_t sc;
    tk_scene_setup(d, &sc);
    out_reset();
    float buf[2 * 128];
    for (int b = 0; b < 16 * TK_FS / 128; ++b) {
        tk_scene_events(d, &sc, 128);
        ec_dsp_process(d, buf, 128);
        out_push(buf, 128);
    }
    save("11_six_tracks_busy_scene.wav");
    tk_scene_free(&sc);
    free(d);
}

int main(void)
{
    printf("rendering demos to results/dsp/\n");
    g_l120 = tk_drum_loop(120.f, &g_f120);
    g_l100 = tk_drum_loop(100.f, &g_f100);
    S120 = (ec_sample_t){g_l120, g_f120, (float)TK_FS, 2, 120.f};
    S100 = (ec_sample_t){g_l100, g_f100, (float)TK_FS, 2, 100.f};
    tk_write_wav_s16("results/dsp/01_source_loop_120bpm.wav", g_l120, g_f120);
    tk_write_wav_s16("results/dsp/01_source_loop_100bpm.wav", g_l100, g_f100);
    printf("  results/dsp/01_source_loop_*.wav\n");
    demo_tape();
    demo_tempo();
    demo_scrub();
    demo_granular();
    demo_chopped();
    demo_loop_trig();
    demo_grid_slice();
    demo_slice_poly();
    demo_filters();
    demo_mix();
    free(g_out);
    free(g_l120);
    free(g_l100);
    return 0;
}
