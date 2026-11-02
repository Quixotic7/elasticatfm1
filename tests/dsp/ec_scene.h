/*
 * ec_scene.h -- the "all six tracks busy" scene shared by the long-run test and
 * the benchmark: tape, tempo_varispeed, granular (density 16), slice_poly with
 * 8 sustained voices, chopped, grid_slice; six different filter machines.
 * A tiny step sequencer emulation queues sample-accurate events per block.
 */
#ifndef EC_SCENE_H
#define EC_SCENE_H
#include "ec_testkit.h"

typedef struct {
    int16_t *loop120, *loop100;
    uint32_t f120, f100;
    ec_sample_t s120, s100;
    double next_step;
    int step;
    double clock;
} tk_scene_t;

static void tk_scene_setup(ec_dsp_t *d, tk_scene_t *sc)
{
    memset(sc, 0, sizeof *sc);
    sc->loop120 = tk_drum_loop(120.f, &sc->f120);
    sc->loop100 = tk_drum_loop(100.f, &sc->f100);
    sc->s120 = (ec_sample_t){sc->loop120, sc->f120, (float)TK_FS, 2, 120.f};
    sc->s100 = (ec_sample_t){sc->loop100, sc->f100, (float)TK_FS, 2, 100.f};
    ec_track_params_t p;

    ec_track_params_default(&p);          /* T1 tape + classic LP with env */
    p.filter_cutoff = 1800.f; p.filter_res = 0.5f; p.filter_env_depth = 0.4f;
    p.filter_env_mode = EC_ENV_ADSR; p.filter_env_decay = 0.2f; p.filter_env_sustain = 0.2f;
    p.filter_env_release = 0.1f; p.amp = 0.4f;
    ec_dsp_track_params(d, 0, &p); ec_dsp_set_sample(d, 0, &sc->s120);

    ec_track_params_default(&p);          /* T2 tempo_varispeed + ladder */
    p.mode = EC_WARP_TEMPO_VARISPEED; p.filter_machine = EC_FILT_LADDER;
    p.filter_cutoff = 900.f; p.filter_res = 0.7f; p.filter_drive = 0.3f; p.amp = 0.5f; p.pan = -0.5f;
    ec_dsp_track_params(d, 1, &p); ec_dsp_set_sample(d, 1, &sc->s100);

    ec_track_params_default(&p);          /* T3 granular + svf BP */
    p.mode = EC_WARP_GRANULAR; p.grain_density = 16.f; p.grain_size = 0.1f;
    p.grain_jitter = 0.05f; p.grain_speed = 0.5f; p.pitch = 7.f;
    p.filter_machine = EC_FILT_SVF; p.filter_type = EC_FTYPE_BP; p.filter_cutoff = 1500.f;
    p.filter_res = 0.3f; p.amp = 0.25f; p.pan = 0.5f;
    ec_dsp_track_params(d, 2, &p); ec_dsp_set_sample(d, 2, &sc->s120);

    ec_track_params_default(&p);          /* T4 slice_poly (8 sustained voices) + morph */
    p.machine = EC_MACH_SLICE_POLY; p.slice_play_mode = EC_SLICE_LOOP;
    p.filter_machine = EC_FILT_MORPHING; p.filter_morph = 0.3f; p.filter_cutoff = 3000.f;
    p.amp = 0.2f;
    ec_dsp_track_params(d, 3, &p); ec_dsp_set_sample(d, 3, &sc->s120);

    ec_track_params_default(&p);          /* T5 chopped + comb */
    p.mode = EC_WARP_CHOPPED; p.chop_slice_len = 1.f; p.filter_machine = EC_FILT_COMB;
    p.filter_cutoff = 220.f; p.filter_res = 0.6f; p.filter_mix = 0.5f; p.amp = 0.4f;
    ec_dsp_track_params(d, 4, &p); ec_dsp_set_sample(d, 4, &sc->s100);

    ec_track_params_default(&p);          /* T6 grid_slice one-shots + formant */
    p.machine = EC_MACH_GRID_SLICE; p.filter_machine = EC_FILT_FORMANT;
    p.filter_morph = 0.6f; p.filter_cutoff = 1200.f; p.filter_res = 0.5f; p.amp = 0.5f;
    ec_dsp_track_params(d, 5, &p); ec_dsp_set_sample(d, 5, &sc->s120);

    ec_dsp_set_tempo(d, 120.f);
    ec_dsp_transport(d, 0, 1, 1);
    sc->next_step = 0.0;
}

/* queue the events for the block [clock, clock+frames) */
static void tk_scene_events(ec_dsp_t *d, tk_scene_t *sc, int frames)
{
    double step_len = 15.0 * TK_FS / ec_dsp_tempo(d);
    while (sc->next_step < sc->clock + frames) {
        int off = (int)(sc->next_step - sc->clock);
        if (off < 0) off = 0;
        int s = sc->step % 16;
        ec_event_t e;
        if (s % 4 == 0) {
            for (int t = 0; t < 3; ++t) {
                e = tk_ev(off, EC_EV_TRIG, t); e.length = (int)(step_len * 3); ec_dsp_event(d, &e);
            }
            e = tk_ev(off, EC_EV_TRIG, 4); e.length = (int)(step_len * 4); ec_dsp_event(d, &e);
        }
        if (s == 0) {   /* 8 sustained slice voices, one bar long */
            for (int k = 0; k < 8; ++k) {
                e = tk_ev(off, EC_EV_TRIG, 3);
                e.slice = 1 + (k * 2 + sc->step / 16) % 16;
                e.pitch = (float)((k * 5) % 12 - 5);
                e.length = (int)(step_len * 15.5);
                e.velocity = 0.7f;
                ec_dsp_event(d, &e);
            }
        }
        e = tk_ev(off, EC_EV_TRIG, 5);
        e.slice = 1 + (s * 7) % 16;
        e.velocity = (s % 4 == 0) ? 1.f : 0.6f;
        ec_dsp_event(d, &e);
        sc->step++;
        sc->next_step += step_len;
    }
    sc->clock += frames;
}

static void tk_scene_free(tk_scene_t *sc)
{
    free(sc->loop120);
    free(sc->loop100);
}
#endif
