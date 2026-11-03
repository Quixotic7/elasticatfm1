/* SPDX-License-Identifier: GPL-3.0-only */
/* The sequencer: a C port of Elasticat's lib/sequencer/track_sequencer.lua + the transport / pattern half of
 * lib/grid_sequencer.lua, sample-accurate.
 *
 * Timing is derived, never accumulated (as the Lua): one shared beat clock, beats(t) = beat_base + (t - sample_base)
 * / samples_per_beat, rebased only when the tempo changes; a track's step n starts at the first sample whose beat is
 * at or past boundary_beats(n) (swing shifts odd boundaries). ec_seq_render() splits the audio at every due moment
 * (a step boundary, a p-lock window's end, a ratchet hit, the global pattern boundary) so events AND parameter
 * changes land on their exact sample: the engine is called for the frames up to it, then the events are queued at
 * offset 0 of the next call.
 *
 * The base-value resolver (docs/BASE_VALUE_RESOLVER.md, without the crossfader until E5): what a track sends is
 * the firing step's p-lock while its note-length window lasts (the "holds"), else the track value. Region locks
 * (loop_start / loop_end) ride the engine's own seq region layer (EC_EV_TRIG start/end + EC_EV_REGION_RELEASE when
 * the window ends); live loop keys are the engine's keys layer (EC_EV_LOOP_KEY). Nothing here writes a track value.
 *
 * Platform free and engine free: the engine is reached through the sink, so tests can record the events. */
#ifndef EC_SEQ_H
#define EC_SEQ_H
#include <stdint.h>

#include "ec_dsp.h"
#include "ec_project.h"

typedef struct {
    void *ctx;
    void (*event)(void *ctx, const ec_event_t *ev);                       /* for the next process() call */
    void (*params)(void *ctx, int track, const ec_track_params_t *p);     /* the resolved params, when changed */
    void (*process)(void *ctx, float *out_lr, int frames);                /* render (out_lr may be NULL) */
    /* 0.5.0: the engine's mod sources of a track now (EC_MSRC_N values; NULL: none) for the matrix's generic
     * destinations (ec_mtx.h) */
    void (*sources)(void *ctx, int track, float *src);
} ec_seq_sink_t;

typedef struct {
    int64_t position;              /* last fired 0-based clock step position; -1 = not primed */
    int play_index;                /* 0-based step index in the pattern */
    int pass;                      /* pattern pass (A:B, 1ST) */
    int8_t last_cond;              /* PRE / !PRE: -1 none, 0 / 1 */
    uint8_t hold_on[EC_NP];        /* param_lock_holds */
    float hold_val[EC_NP];
    uint64_t hold_exp[EC_NP];      /* sample time the window ends (UINT64_MAX: never) */
    uint64_t hold_min;             /* the earliest of them (UINT64_MAX: none) */
    /* ratchets of the last fired step */
    int rat_left, rat_k, rat_index;
    uint64_t rat_t0;
    double rat_iv;
    /* the last values sent */
    ec_track_params_t sent;
    int sent_ok;
    int dirty;
    int mtx_gen;                   /* 0.5.0: a matrix slot aims at a generic destination (resent every chunk) */
    /* live */
    float live_pitch;              /* keyboard transpose while a key is held */
    int live_pitch_on;
    uint32_t live_keys;            /* held live loop keys (bit k-1), for the UI */
    uint32_t pads;                 /* held slice pads */
    uint32_t fired_slices;         /* the last step's slices (UI) */
    uint64_t last_fire;            /* sample time of the last fired trig (UI activity) */
    int fired;                     /* trigs fired so far (tests, ui_info) */
} ec_seq_track_t;

typedef struct {
    ec_project_t *proj;
    ec_seq_sink_t sink;
    double fs;
    uint64_t now;                  /* samples rendered since init */
    int playing;
    int paused;                    /* stopped by PLAY with the position kept (resume continues) */
    double beat_base;
    uint64_t sample_base;
    double paused_beats;
    double bpm;                    /* the tempo the clock runs at */
    int64_t global_cycle;
    int global_pos;                /* 0-based position in the global length */
    int fill_held, fill_latched;
    int pending;                   /* a sequential switch queued (-1 none) */
    int temp_return;               /* temp jump: the pattern to return to (-1 none) */
    uint32_t rng;
    int switches;                  /* pattern switches applied (tests) */
    int stops;                     /* consecutive stops (stop twice = panic) */
    ec_seq_track_t tr[EC_NTRACK];
} ec_seq_t;

void ec_seq_init(ec_seq_t *s, ec_project_t *proj, const ec_seq_sink_t *sink, double fs);
/* render frames (out_lr: frames * 2 floats, handed to sink->process in pieces) */
void ec_seq_render(ec_seq_t *s, float *out_lr, int frames);

/* transport */
void ec_seq_start(ec_seq_t *s);            /* from the top (or resume after a pause) */
void ec_seq_pause(ec_seq_t *s);            /* PLAY while playing: the position is kept */
void ec_seq_stop(ec_seq_t *s);             /* stop + reset to step 1 (twice: panic) */
void ec_seq_toggle(ec_seq_t *s);           /* PLAY: start / pause / resume */
void ec_seq_set_bpm(ec_seq_t *s, float bpm);
double ec_seq_beats(const ec_seq_t *s);    /* the clock now (frozen while paused) */

/* patterns: switch to idx under the project's quantization (stopped: at once) */
void ec_seq_request_pattern(ec_seq_t *s, int idx);
void ec_seq_reload(ec_seq_t *s);           /* E5: the project was replaced (recall): reload, from step 1 */
void ec_seq_set_fill(ec_seq_t *s, int held, int latched);
int ec_seq_fill_active(const ec_seq_t *s);

/* edits: the track's params / steps / slot changed (resolve and resend before the next frames). RATE or swing
 * changes re-derive the positions (no catch-up burst), glen re-derives the global cycle */
void ec_seq_touch(ec_seq_t *s, int track);
void ec_seq_touch_all(ec_seq_t *s);
void ec_seq_timing_changed(ec_seq_t *s);

/* live gestures on a track */
void ec_seq_loop_key(ec_seq_t *s, int track, int key, int pressed);              /* 1..32 */
void ec_seq_slice_pad(ec_seq_t *s, int track, int slice, int pressed, float vel);/* 1..32 */
void ec_seq_keyboard(ec_seq_t *s, int track, float semis, int pressed, float vel);

/* reads */
float ec_seq_value(const ec_seq_t *s, int track, int id);   /* the resolved ("actual") value */
int ec_seq_step(const ec_seq_t *s, int track);              /* the playing step index (0-based), -1 stopped */
int ec_seq_pattern_steps(const ec_seq_t *s, int track);
double ec_seq_step_samples(const ec_seq_t *s, int track);   /* one step at the track's rate */
/* 0-based clock position at `beats` for the track (swing aware); boundary of position n in beats */
int64_t ec_seq_position_at(const ec_seq_t *s, int track, double beats);
double ec_seq_boundary_beats(const ec_seq_t *s, int track, int64_t n);

#endif
