/*
 * bench.c -- native CPU benchmark of the busy six-track scene (ec_scene.h):
 * tape, tempo_varispeed, granular (density 16), slice_poly (8 sustained
 * voices), chopped, grid_slice; six different filter machines.
 * Prints the share of one core at 44.1 kHz and a per-track breakdown.
 */
#include <time.h>
#include "ec_scene.h"

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}

static double run(ec_dsp_t *d, tk_scene_t *sc, int seconds)
{
    float buf[2 * 128];
    int blocks = seconds * TK_FS / 128;
    double t0 = now_s();
    for (int b = 0; b < blocks; ++b) {
        tk_scene_events(d, sc, 128);
        ec_dsp_process(d, buf, 128);
    }
    return now_s() - t0;
}

int main(void)
{
    const int secs = 60;
    ec_dsp_t *d = tk_engine();
    tk_scene_t sc;
    tk_scene_setup(d, &sc);
    run(d, &sc, 2);   /* warm up */
    double best = 1e9;
    for (int r = 0; r < 3; ++r) { double t = run(d, &sc, secs); if (t < best) best = t; }
    double pct = 100.0 * best / secs;
    printf("busy 6-track scene: %.3f s CPU for %d s audio = %.2f %% of one core (%.0f x real time)\n",
           best, secs, pct, secs / best);
    printf("voices sounding at the end: %d\n", ec_dsp_total_voices(d));

    /* per-track share: mute the scene down to one track at a time */
    static const char *names[6] = {"tape+classic", "tempo_varispeed+ladder", "granular16+svf",
                                   "slice_poly x8+morph", "chopped+comb", "grid_slice+formant"};
    for (int keep = 0; keep < 6; ++keep) {
        ec_dsp_t *e = tk_engine();
        tk_scene_t s2;
        tk_scene_setup(e, &s2);
        for (int t = 0; t < 6; ++t) {
            if (t == keep) continue;
            ec_track_params_t p = *ec_dsp_get_track_params(e, t);
            p.machine = EC_MACH_GRID_SLICE;   /* idle source: no reader, no trigs land */
            p.slice_count = 1;
            ec_dsp_track_params(e, t, &p);
            ec_dsp_set_sample(e, t, NULL);
        }
        run(e, &s2, 2);
        double t1 = run(e, &s2, 20);
        printf("  only %-24s %.2f %%\n", names[keep], 100.0 * t1 / 20.0);
        tk_scene_free(&s2);
        free(e);
    }
    tk_scene_free(&sc);
    free(d);

    /* E4: the same scene with an insert on every track, both sends (REVERB, DELAY) fed by every track, a master
     * COMP and an LFO per track */
    {
        static const int ins[6] = {EC_FX_COMP, EC_FX_TAPE_ECHO, EC_FX_CHORUS, EC_FX_PHASER, EC_FX_DESTROY,
                                   EC_FX_RINGS};
        ec_dsp_t *e = tk_engine();
        tk_scene_t s3;
        ec_fx_params_t fp;
        tk_scene_setup(e, &s3);
        for (int t = 0; t < 6; ++t) {
            ec_track_params_t p = *ec_dsp_get_track_params(e, t);
            p.fx.machine = ins[t];
            p.fx.mix = 0.6f;
            p.fx.destroy_warble = p.fx.destroy_fold = p.fx.destroy_sat = 0.5f;   /* DESTROY: every stage on */
            p.fx.destroy_crush = p.fx.destroy_srr = 0.3f;
            p.fx.drive = 0.4f;
            p.fx.rings_mode = 1;                                                   /* RINGS: the frequency shifter */
            p.send1_level = 0.3f;
            p.send2_level = 0.2f;
            p.mod.slot[0].src = 1; p.mod.slot[0].bus = EC_MOD_CUTOFF; p.mod.slot[0].depth = 0.3f;
            p.mod.lfo[0].depth = 0.5f;
            ec_dsp_track_params(e, t, &p);
        }
        ec_fx_params_default(&fp);
        fp.machine = EC_FX_REVERB; fp.reverb_size = 0.9f;
        ec_dsp_bus_params(e, 0, &fp);
        fp.machine = EC_FX_DELAY; fp.delay_feedback = 0.6f;
        ec_dsp_bus_params(e, 1, &fp);
        fp.machine = EC_FX_COMP;
        ec_dsp_bus_params(e, 2, &fp);
        run(e, &s3, 2);
        double best3 = 1e9;
        for (int r = 0; r < 3; ++r) { double t = run(e, &s3, secs); if (t < best3) best3 = t; }
        double pct3 = 100.0 * best3 / secs;
        printf("E4 busy scene (6 inserts COMP/TAPE ECHO/CHORUS/PHASER/DESTROY/RINGS-shift, S1 REVERB, S2 DELAY,\n"
               "   master COMP, an LFO per track): %.2f %% of one core (%.0f x real time); FX + MOD add %.2f %%\n",
               pct3, secs / best3, pct3 - pct);
        printf("   wasm3 estimate (22x native): %.1f %% of real time -> %.1f x real time\n", pct3 * 22.0,
               100.0 / (pct3 * 22.0));
        tk_scene_free(&s3);
        free(e);
    }
    return 0;
}
