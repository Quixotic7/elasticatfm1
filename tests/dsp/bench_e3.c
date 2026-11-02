/*
 * bench_e3.c -- E3 CPU: the busy six-track scene (ec_scene.h) with its loop
 * tracks switched to the heaviest new warp modes, then every track in each
 * mode alone, and six spectral tracks at once (the FFT budget question).
 * Prints the share of one core at 44.1 kHz (wasm3 costs about 22x that).
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

static void set_mode(ec_dsp_t *d, int t, int mode)
{
    ec_track_params_t p = *ec_dsp_get_track_params(d, t);
    p.machine = EC_MACH_LOOP;
    p.mode = mode;
    p.mode_macro = 0.6f;
    p.harm_interval[0] = 7.f; p.harm_interval[1] = 12.f; p.harm_interval[2] = -5.f;
    p.grain_density = 16.f;         /* random_ola overlap / gstretch SMTH */
    p.wsola_window = 0.2f;
    p.wt_lfo_rate = 300.f; p.wt_lfo_depth = 0.6f;
    p.spectral_blur = 0.5f; p.formant_shift = 4.f; p.pitch = 3.f;
    ec_dsp_track_params(d, t, &p);
}

static double pct(double t, int secs) { return 100.0 * t / secs; }

int main(void)
{
    static const int heavy[6] = {EC_WARP_HARMONIZER, EC_WARP_RANDOM_OLA, EC_WARP_SPECTRAL_FREEZE, EC_WARP_FORMANT,
                                 EC_WARP_WAVETABLE, EC_WARP_GSTRETCH2};
    static const char *hn[6] = {"harmonizer x3", "random_ola dens16", "spectral_freeze", "formant",
                                "wavetable lfo300", "gstretch2"};
    const int secs = 30;
    {
        ec_dsp_t *d = tk_engine();
        tk_scene_t sc;
        tk_scene_setup(d, &sc);
        for (int t = 0; t < 6; ++t) set_mode(d, t, heavy[t]);
        run(d, &sc, 2);
        double best = 1e9;
        for (int r = 0; r < 3; ++r) { double x = run(d, &sc, secs); if (x < best) best = x; }
        printf("E3 heavy scene (6 loop tracks: %s, %s, %s, %s, %s, %s):\n  %.2f %% of one core (%.0f x real time; "
               "wasm3 ~%.0f %%)\n", hn[0], hn[1], hn[2], hn[3], hn[4], hn[5], pct(best, secs), secs / best,
               22.0 * pct(best, secs));
        tk_scene_free(&sc);
        free(d);
    }
    {
        ec_dsp_t *d = tk_engine();
        tk_scene_t sc;
        tk_scene_setup(d, &sc);   /* the E2 busy scene, its four loop tracks in the heaviest E3 modes */
        set_mode(d, 0, EC_WARP_HARMONIZER);
        set_mode(d, 1, EC_WARP_RANDOM_OLA);
        set_mode(d, 2, EC_WARP_SPECTRAL_FREEZE);
        set_mode(d, 4, EC_WARP_FORMANT);
        run(d, &sc, 2);
        double t1 = run(d, &sc, secs);
        printf("busy scene, T1 harmonizer / T2 random_ola / T3 freeze / T5 formant, T4 slice_poly x8, T6 grid:\n"
               "  %.2f %% of one core (wasm3 ~%.0f %%)\n", pct(t1, secs), 22.0 * pct(t1, secs));
        tk_scene_free(&sc);
        free(d);
    }
    {
        ec_dsp_t *d = tk_engine();
        tk_scene_t sc;
        tk_scene_setup(d, &sc);
        for (int t = 0; t < 6; ++t) set_mode(d, t, t % 2 ? EC_WARP_FORMANT : EC_WARP_SPECTRAL_FREEZE);
        run(d, &sc, 2);
        double t1 = run(d, &sc, secs);
        printf("six spectral tracks: %.2f %% of one core (wasm3 ~%.0f %%)\n", pct(t1, secs), 22.0 * pct(t1, secs));
        tk_scene_free(&sc);
        free(d);
    }
    printf("one track alone in each mode:\n");
    static const int all[9] = {EC_WARP_DOMINO, EC_WARP_RANDOM_OLA, EC_WARP_PITCH_CORRECTED, EC_WARP_HARMONIZER,
                               EC_WARP_WAVETABLE, EC_WARP_SPECTRAL_FREEZE, EC_WARP_FORMANT, EC_WARP_GSTRETCH,
                               EC_WARP_GSTRETCH2};
    static const char *an[9] = {"domino", "random_ola", "pitch_corrected", "harmonizer", "wavetable",
                                "spectral_freeze", "formant", "gstretch", "gstretch2"};
    for (int m = 0; m < 9; ++m) {
        ec_dsp_t *e = tk_engine();
        tk_scene_t s2;
        tk_scene_setup(e, &s2);
        for (int t = 1; t < 6; ++t) {
            ec_track_params_t p = *ec_dsp_get_track_params(e, t);
            p.machine = EC_MACH_GRID_SLICE;
            p.slice_count = 1;
            ec_dsp_track_params(e, t, &p);
            ec_dsp_set_sample(e, t, NULL);
        }
        set_mode(e, 0, all[m]);
        run(e, &s2, 1);
        double t1 = run(e, &s2, 10);
        printf("  %-16s %.3f %%\n", an[m], pct(t1, 10));
        tk_scene_free(&s2);
        free(e);
    }
    return 0;
}
