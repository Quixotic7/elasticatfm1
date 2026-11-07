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
    /* 0.6.0: the pool slot (1-based) the track's engine plays changed: a trig with an EC_P_SLOT lock plays that
     * slot's sample (it stays until a trig without one, or a stop, gives the track's own slot back). NULL: none */
    void (*slot)(void *ctx, int track, int slot);
} ec_seq_sink_t;

typedef struct {
    int64_t position;              /* last fired 0-based clock step position; -1 = not primed */
    double org_beats, org_pos, org_bps;   /* 0.6.88: where its rate took over (song beats, the step position there,
                                     * the step length since): a RATE change keeps the playhead where it is */
    int play_index;                /* 0-based step index in the pattern */
    int pass;                      /* pattern pass (A:B, 1ST) */
    int8_t last_cond;              /* PRE / !PRE: -1 none, 0 / 1 */
    uint8_t hold_on[EC_NP];        /* param_lock_holds (0.6.4: sticky, as Elektron's: held until a trig or lock changes
                                    * them; only the region locks keep their note window) */
    float hold_val[EC_NP];
    uint64_t hold_exp[EC_NP];      /* sample time the window ends (UINT64_MAX: never) */
    /* 0.6.4: slides (EC_P_SLIDE): param id ramps hold_val -> slide_to over [slide_t0, slide_t1) */
    uint8_t slide_on[EC_NP];
    float slide_to[EC_NP];
    uint64_t slide_t0[EC_NP], slide_t1[EC_NP];
    int sliding;                   /* params sliding (resent every chunk while > 0) */
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
    float live_pitch;              /* keyboard transpose while a key is held (PITCH + the key's, at the press) */
    int live_pitch_on;
    float live_semis;              /* 0.6.1: the key's semitones (the pitch follows PITCH while held) */
    int kb_down;                   /* 0.6.1: keyboard keys held (stopped: the loop auditioned until the last) */
    int live_range;                /* 0.6.77: the range keys held: the range ignores the steps' range locks */
    uint32_t live_keys;            /* held live loop keys (bit k-1), for the UI */
    int aud_step;                  /* 0.6.30: the selected step (-1 none): stopped, the region / range values heard
                                    * are its locks (the pads and keys audition in its range) */
    uint32_t pads;                 /* held slice pads */
    uint32_t fired_slices;         /* the last step's slices (UI) */
    uint64_t last_fire;            /* sample time of the last fired trig (UI activity) */
    int fired;                     /* trigs fired so far (tests, ui_info) */
    /* 0.6.0: micro timing (ec_seq.c "micro timing") */
    int64_t fpos;                  /* the next clock position to fire (-1 none) */
    int fire_pass;                 /* the firing step's pattern pass (trig conditions) */
    int fire_reset;                /* the next fire is the first after a (re)start */
    int64_t skip_pos;              /* ec_seq_rec_consume: this position does not fire (-1 none) */
    int slot_lock;                 /* 0.6.0: the slot the last trig locked (1-based), 0 = the track's own */
} ec_seq_track_t;

/* 0.6.0: a metronome beat in the block ec_seq_render() just rendered (ec_seq_block_beats) */
typedef struct {
    uint32_t offset;               /* its frame in the block */
    int32_t beat;                  /* the clock's beat: 0 = the transport's / the pattern's start (a pattern switch
                                    * restarts the clock), negative = a count-in beat (-n .. -1) */
    uint8_t accent;                /* the first beat of a bar (4/4: beat % 4 == 0; the first count-in beat of a bar) */
    uint8_t countin;               /* 1: a count-in beat (the transport is not running yet) */
} ec_seq_beat_t;
#define EC_SEQ_BEATS_MAX 16

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
    /* 0.6.0: live recording */
    double song_base;              /* beats played before the last clock restart (ec_seq_song_beats) */
    uint64_t start_at;             /* an armed start (ec_seq_count_in / ec_seq_start_at): its sample, UINT64_MAX none */
    uint64_t ci_t0;                /* the count-in's first beat */
    double ci_spb;                 /* its samples per beat */
    int ci_n;                      /* its beats */
    ec_seq_beat_t beat[EC_SEQ_BEATS_MAX];   /* the last block's beats */
    int nbeat;
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
void ec_seq_audition_step(ec_seq_t *s, int track, int idx);   /* 0.6.30: the selected step, -1 none */
void ec_seq_touch_all(ec_seq_t *s);
void ec_seq_timing_changed(ec_seq_t *s);

/* live gestures on a track */
void ec_seq_loop_key(ec_seq_t *s, int track, int key, int pressed);              /* 1..32 */
void ec_seq_slice_pad(ec_seq_t *s, int track, int slice, int pressed, float vel);/* 1..32 */
void ec_seq_keyboard(ec_seq_t *s, int track, float semis, int pressed, float vel);
void ec_seq_chord_key(ec_seq_t *s, int track, int slot, float semis, int pressed, float vel);   /* 0.6.82: a POLY TRIG chord key */
void ec_seq_keyboard_retune(ec_seq_t *s, int track, float semis);   /* 0.6.67: a key still held takes the note back */

/* ---- 0.6.0: live recording (docs/UX-V2.md 4). The app owns the modes (CAPTURE / FIXED / OVERDUB), what gets
 * written and the click (ec_click_*); the sequencer gives it the timing. ---- */
/* the step a hit "now" belongs to on a track: the nearest step boundary of the track's own clock (its rate, swing,
 * length and page loop), as the 0-based step index, and the hit's offset from that boundary in steps (-0.5 .. 0.5:
 * negative = early). Write the offset as an EC_P_MICRO lock (ec_micro_units), or drop it for QUANTIZE hard.
 * Returns 0, or -1 while the transport is not running (nothing to quantize to) */
int ec_seq_rec_quantize(const ec_seq_t *s, int track, int *step, float *offset);
static inline int ec_micro_units(float offset)       /* a step offset -> EC_P_MICRO's units, rounded, clamped */
{
    const float u = offset * EC_MICRO_DIV;
    const int n = (int)(u < 0 ? u - 0.5f : u + 0.5f);
    return n < -EC_MICRO_DIV / 2 ? -EC_MICRO_DIV / 2 : n > EC_MICRO_DIV / 2 ? EC_MICRO_DIV / 2 : n;
}
/* a trig was just recorded on step `step` of the track (heard live already): if that step is the next to fire on
 * this pass (an early hit quantized forward), it does not fire this time. Locks-only writes need no call */
void ec_seq_rec_consume(ec_seq_t *s, int track, int step);
/* the metronome: the beats that fell in the block the last ec_seq_render() call rendered, in order, with their frame
 * offsets (sample-accurate: a beat lands on the same frame as the step on it). Count-in beats too. Returns n <= max
 * (at most EC_SEQ_BEATS_MAX per block) */
int ec_seq_block_beats(const ec_seq_t *s, ec_seq_beat_t *out, int max);
/* COUNT-IN: start from the top after `beats` beats at the current BPM, counted from the next frame rendered; the
 * count-in beats come through ec_seq_block_beats (countin = 1). 0 beats: ec_seq_start now. Cancelled by
 * ec_seq_stop, replaced by ec_seq_start / another arm */
void ec_seq_count_in(ec_seq_t *s, int beats);
/* start from the top at frame `frame` of the next ec_seq_render() block (sample accurate; 0 = its first frame) */
void ec_seq_start_at(ec_seq_t *s, uint32_t frame);
int ec_seq_counting_in(const ec_seq_t *s);   /* count-in beats still to come (0: none armed / started) */
/* beats played since the transport started from the top (pattern switches do not reset it; paused time is not
 * counted; 0 when stopped): FIXED's bar counter = floor(ec_seq_song_beats / 4) */
double ec_seq_song_beats(const ec_seq_t *s);

/* reads */
int ec_seq_track_slot(const ec_seq_t *s, int track);        /* 0.6.0: the slot (1-based) the engine plays (a lock) */
/* 0.6.4: a track value turned by hand: it is heard at once (a sticky lock or slide on it lets go, as Elektron's) */
void ec_seq_param_edited(ec_seq_t *s, int track, int id);
float ec_seq_value(const ec_seq_t *s, int track, int id);   /* the resolved ("actual") value */
int ec_seq_step(const ec_seq_t *s, int track);              /* the playing step index (0-based), -1 stopped */
int ec_seq_pattern_steps(const ec_seq_t *s, int track);
double ec_seq_step_samples(const ec_seq_t *s, int track);   /* one step at the track's rate */
/* 0-based clock position at `beats` for the track (swing aware); boundary of position n in beats */
int64_t ec_seq_position_at(const ec_seq_t *s, int track, double beats);
double ec_seq_step_beats(const ec_seq_t *s, int track);   /* 0.6.59: one step of the track, in beats */
double ec_seq_boundary_beats(const ec_seq_t *s, int track, int64_t n);

#endif
