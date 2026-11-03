/*
 * ec_dsp.h -- the Elasticat FM-1 sound engine (C port of Engine_Elasticat.sc +
 * the compiled ElasticatReader / ElasticatGrains UGens).
 *
 * Pure C99, no platform calls, no allocation: the caller hands the engine one
 * block of memory (ec_dsp_mem_size() bytes) and everything lives inside it.
 * Not thread-safe: call everything from the audio thread (or between blocks).
 *
 * ---------------------------------------------------------------------------
 * Signal flow (one chain per track, 6 tracks, as in the original):
 *
 *   machine source ---------------------------+
 *     loop / loop_trig : warp mode: the reader (tape, tempo_varispeed, + E3 post
 *                        stages pitch_corrected / harmonizer / random_ola /
 *                        gstretch / gstretch2), granular, chopped, domino,
 *                        wavetable, spectral_freeze / formant (ec_warp3.c)
 *                        x amp envelope x play gate x mode-switch gain
 *     grid_slice / slice_poly : slice voices (<= 8 per track, <= 24 total)
 *     neighbor (E3)    : track N-1's whole output (post filter / amp / pan /
 *                        mute; E4: post insert) -- N-1 then no longer reaches
 *                        the master, it is heard only through this track
 *                                             |
 *   FILTER machine (stereo / mid-side, balance-spread cutoffs, drive, env)
 *     classic, morphing, ladder, comb, formant, svf
 *   AMP/PAN stage (Balance2, x2.63 makeup as the SC filter stage)
 *   (MOD: LFOs / mod env / velocity / macros -> pitch, cutoff, res, amp, pan)
 *   insert FX (ec_fx.h)  -> send taps S1 / S2 (pre or post insert, Lag 0.02)
 *   track mix (mute, centre Balance2)  -> master sum
 *   S1 / S2 returns (their FX machines, DUCK keyed by the tracks' sum) -> master
 *   master: master insert -> master volume -> soft limiter -> out
 *   Idle CPU saver: a stopped track whose output stayed below -60 dB for 4 s
 *   is skipped until an event, a param change or the transport wakes it.
 *
 * ---------------------------------------------------------------------------
 * Units.  The parameter struct holds ENGINE units, i.e. what the Lua param
 * actions sent to SuperCollider after their xform (see ec_xf_* helpers below):
 *   - option params take the params_spec.lua 1-based OPTION INDEX
 *     (machine 1..8, mode 1..13, filter_machine 1..6, filter_type 1..4,
 *      env_mode 1 ADSR / 2 AHR, slice_play_mode 1..6, ...);
 *   - "amount" params 0..1, "bipolar" params -1..1, times in seconds,
 *     cutoff in Hz, pitch in semitones;
 *   - REGION points stay in TRACK SPACE 0..128 (the raw loop_start/loop_end
 *     param values). The engine folds in the slot's file trim and the track's
 *     Range itself (Lua map_trim_point), so every region the sequencer sends
 *     (params, step locks, loop keys, slice ranges) is plain 0..128.
 *
 * Times: event offsets and lengths are in SAMPLES at the engine rate.
 *
 * ---------------------------------------------------------------------------
 * params_spec.lua  ->  ec_track_params_t  (xform the sequencer applies)
 *
 *   play               -> play              0/1
 *   mute               -> mute              0/1
 *   machine            -> machine           option 1..8   (EC_MACH_*)
 *   pitch              -> pitch             st -24..24    (none)
 *   loop_start         -> loop_start        0..128        (NONE: raw track space;
 *   loop_end           -> loop_end          0..128         engine does region_point)
 *   range_start        -> range_start       0..128
 *   range_end          -> range_end         0..128
 *   (slot file trim)   -> trim_start/end    0..1 of the sample (slot metadata)
 *   (slot sample_steps)-> sample_steps      steps the TRIM spans (slot metadata;
 *                                           the engine does trim_scaled_steps)
 *   loop_reverse       -> loop_reverse      0/1
 *   xfade              -> xfade             s 0..0.25
 *   mode               -> mode              option 1..13  (EC_WARP_*)
 *   mode_macro         -> mode_macro        0..1
 *   chop_steps         -> chop_steps        1..64
 *   chop_slice_len     -> chop_slice_len    steps 0.05..32
 *   chop_loop_mode     -> chop_loop_mode    option 1..8
 *   chop_attack        -> chop_attack       s
 *   chop_hold          -> chop_hold         0..1 (gate)
 *   chop_release       -> chop_release      s
 *   grain_size         -> grain_size        s 0.002..0.5
 *   grain_density      -> grain_density     1..64
 *   grain_jitter       -> grain_jitter      0..0.25
 *   grain_speed        -> grain_speed       0..4
 *   grain_speed_rand   -> grain_speed_rand  0..1
 *   grain_direction    -> grain_direction   0..1
 *   warp_rate          -> warp_rate         x 0.0625..8
 *   slice_count        -> slice_count       1..32
 *   slice_play_mode    -> slice_play_mode   option 1..6 (1shot hold loop cont pong cloop)
 *   slice_reverse      -> slice_reverse     0/1
 *   slice_sync         -> slice_sync        0/1
 *   slice_rate         -> slice_rate        x 0.125..8
 *   filter_machine     -> filter_machine    option 1..6 (EC_FILT_*)
 *   filter_type        -> filter_type       option 1..4 (LP HP BP NOTCH)
 *   filter_cutoff      -> filter_cutoff     Hz   ec_xf_cutoff_hz(0..127)
 *   filter_res         -> filter_res        0..1 ec_xf_amount
 *   filter_drive       -> filter_drive      0..1 ec_xf_amount
 *   filter_morph       -> filter_morph      0..1 ec_xf_morph(0..128)
 *   filter_balance     -> filter_balance    -1..1 ec_xf_bipolar(0..128)
 *   filter_mix         -> filter_mix        0..1 ec_xf_amount
 *   filter_mix_mode    -> filter_mix_mode   option 1 stereo / 2 mid-side
 *   filter_env_mode    -> filter_env_mode   option 1 ADSR / 2 AHR
 *   filter_env_attack  -> filter_env_attack s    ec_xf_env_seconds(v, env_range)
 *   filter_env_decay   -> filter_env_decay  s    ec_xf_env_seconds
 *   filter_env_sustain -> filter_env_sustain 0..1 ec_xf_amount
 *   filter_env_release -> filter_env_release s   ec_xf_env_seconds (128 = INF)
 *   filter_env_hold    -> filter_env_hold   s    ec_xf_env_seconds (128 = INF)
 *   filter_env_depth   -> filter_env_depth  -1..1 ec_xf_bipolar
 *   env_mode           -> env_mode          option 1 ADSR / 2 AHR
 *   env_attack         -> env_attack        s    ec_xf_env_seconds
 *   env_decay          -> env_decay         s    ec_xf_env_seconds
 *   env_sustain        -> env_sustain       0..1 ec_xf_amount
 *   env_release        -> env_release       s    ec_xf_env_seconds (128 = INF)
 *   env_hold           -> env_hold          s    ec_xf_env_seconds (128 = INF)
 *   amp                -> amp               0..1 (x/127 * slot gain, Lua "amp" xform)
 *   pan                -> pan               -1..1 ec_xf_bipolar
 *   portamento         -> portamento        0/1
 *   mode_switch_fade   -> mode_switch_fade  s 0.001..0.25
 *   loop_division      -> loop_division     2..32 (even)
 *   (global playhead return option) -> playhead_return  1 return / 2 boomerang / 3 reset
 *   send_tap / send1_level / send2_level -> option 1..2 / 0..1 / 0..1
 *   fx_insert1_machine + the FX params -> fx (ec_fx_params_t, see ec_fx.h)
 *   lfo* / menv_* / mvel_* / macroN_* -> mod (ec_mod_params_t, see ec_mod.h)
 *   (engine \trFilterSlew)           -> filter_slew  s (default 0.09)
 *   env_range          -> (Lua only: feeds ec_xf_env_seconds)
 *   trig_* / slice_ratchet / slice_hold* / pattern_steps / default_* ->
 *                         sequencer-side, not engine params.
 *
 *   (E3) harm_interval[3], wsola_window, wsola_search, pv_window, pv_dispersion,
 *   wt_window, wt_cycle, wt_lfo_rate, freeze_amount, spectral_blur, formant_shift
 *                      -> same names, spec units (none)
 *   (E3) wt_lfo_depth  -> wt_lfo_depth  -1..1 ec_xf_bipolar(0..128)
 *   (E3) wt_lfo_shape  -> wt_lfo_shape  option 1..5
 *   (E4) insert FX, sends S1/S2, master insert, MOD: see ec_fx.h / ec_mod.h.
 */
#ifndef EC_DSP_H
#define EC_DSP_H

#include <stddef.h>
#include <stdint.h>

#include "ec_fx.h"    /* E4 */
#include "ec_mod.h"   /* E4 */

#ifdef __cplusplus
extern "C" {
#endif

#define EC_TRACKS            6
#define EC_MAX_VOICES_TRACK  8    /* "poly 8" per-track slice voice cap */
#define EC_MAX_VOICES        24   /* global slice voice pool cap (oldest stolen) */
#define EC_MAX_EVENTS        256  /* events queued per ec_dsp_process block */
#define EC_MAX_BLOCK         1024 /* any frames count works; internally chunked */

/* machine (params_spec "machine" option index) */
enum {
    EC_MACH_LOOP = 1, EC_MACH_LOOP_TRIG = 2, EC_MACH_GRID_SLICE = 3,
    EC_MACH_RAZOR_SLICE = 4,   /* grid_slice voicing, explicit per-slice ranges (seq/ec_razor) */
    EC_MACH_SLICE_POLY = 5,
    EC_MACH_RAZOR_POLY = 6,    /* slice_poly voicing, explicit per-slice ranges */
    EC_MACH_NEIGHBOR = 7,      /* the left track's output as the source (tracks 2-6) */
    EC_MACH_INPUT = 8          /* E6: the live input (ec_dsp_input) as the source */
};

/* warp mode (params_spec "mode" option index; modeNames order in the SC engine) */
enum {
    EC_WARP_TAPE = 1, EC_WARP_TEMPO_VARISPEED = 2, EC_WARP_DOMINO = 3,
    EC_WARP_GRANULAR = 4, EC_WARP_RANDOM_OLA = 5, EC_WARP_PITCH_CORRECTED = 6,
    EC_WARP_HARMONIZER = 7, EC_WARP_WAVETABLE = 8, EC_WARP_SPECTRAL_FREEZE = 9,
    EC_WARP_FORMANT = 10, EC_WARP_GSTRETCH = 11, EC_WARP_GSTRETCH2 = 12,
    EC_WARP_CHOPPED = 13,
    EC_WARP_COUNT = 13
};

/* filter machine (params_spec "filter_machine" option index; svf appended) */
enum {
    EC_FILT_CLASSIC = 1, EC_FILT_MORPHING = 2, EC_FILT_LADDER = 3,
    EC_FILT_COMB = 4, EC_FILT_FORMANT = 5, EC_FILT_SVF = 6
};

/* filter_type option index */
enum { EC_FTYPE_LP = 1, EC_FTYPE_HP = 2, EC_FTYPE_BP = 3, EC_FTYPE_NOTCH = 4 };

/* env_mode / filter_env_mode option index */
enum { EC_ENV_ADSR = 1, EC_ENV_AHR = 2 };

/* slice_play_mode option index */
enum {
    EC_SLICE_ONESHOT = 1, EC_SLICE_HOLD = 2, EC_SLICE_LOOP = 3,
    EC_SLICE_CONTINUE = 4, EC_SLICE_PINGPONG = 5, EC_SLICE_CONTLOOP = 6
};

/* playhead return / trig release mode (1-based like the Lua option) */
enum { EC_RETURN_RETURN = 1, EC_RETURN_BOOMERANG = 2, EC_RETURN_RESET = 3 };

/* Envelope time that means "infinite" (Lua ENV_INFINITE_SECONDS). Any time
 * >= EC_ENV_INF_SECONDS / 10 is treated as infinite. */
#define EC_ENV_INF_SECONDS 1000000.0f

/* ---------------------------------------------------------------------------
 * Sample view. The memory is owned by the caller (the sample store). The
 * engine keeps a copy of this struct and reads data[] during ec_dsp_process
 * until ec_dsp_set_sample() is called again for that track (with NULL or
 * another sample): only after that may the memory be freed/reused.
 * The same sample may be assigned to several tracks.
 */
typedef struct ec_sample {
    const int16_t *data;   /* interleaved L,R (frames * 2 values) */
    uint32_t frames;       /* >= 4 to play */
    float    rate;         /* source sample rate in Hz (played at rate/fs) */
    int      channels;     /* 1 = mono source stored as L=R (read once), 2 = stereo */
    float    bpm;          /* optional, informational (0 = unknown) */
} ec_sample_t;

/* ---------------------------------------------------------------------------
 * Per-track parameters, engine units (see the mapping table at the top).
 * Set any time between blocks with ec_dsp_track_params(); the engine smooths
 * what the SC engine smoothed (Lag) and applies the rest at the next block.
 */
typedef struct ec_track_params {
    /* state */
    int   play;               /* 1 = this track follows the transport */
    int   mute;
    /* source */
    int   machine;            /* EC_MACH_* */
    float pitch;              /* semitones */
    float loop_start, loop_end;    /* track space 0..128 */
    float range_start, range_end;  /* 0..128 window inside the trim */
    float trim_start, trim_end;    /* 0..1 of the sample (slot file trim) */
    float sample_steps;       /* steps the trimmed sample spans (tempo fit) */
    int   loop_reverse;
    float xfade;              /* reader crossfade time, s */
    /* warp */
    int   mode;               /* EC_WARP_* */
    float mode_macro;
    int   chop_steps;
    float chop_slice_len;
    int   chop_loop_mode;     /* 1..8 */
    float chop_attack, chop_hold, chop_release;
    float grain_size, grain_density, grain_jitter;
    float grain_speed, grain_speed_rand, grain_direction;
    float warp_rate;
    /* slices */
    int   slice_count;
    int   slice_play_mode;    /* EC_SLICE_* */
    int   slice_reverse;
    int   slice_sync;
    float slice_rate;
    /* filter */
    int   filter_machine;     /* EC_FILT_* */
    int   filter_type;        /* EC_FTYPE_* */
    float filter_cutoff;      /* Hz */
    float filter_res, filter_drive, filter_morph, filter_balance, filter_mix;
    int   filter_mix_mode;    /* 1 stereo, 2 mid-side */
    int   filter_env_mode;    /* EC_ENV_* */
    float filter_env_attack, filter_env_decay, filter_env_sustain;
    float filter_env_release, filter_env_hold, filter_env_depth;
    float filter_slew;        /* s, smoothing of res/pan/amp (SC default 0.09) */
    /* amp */
    int   env_mode;           /* EC_ENV_* */
    float env_attack, env_decay, env_sustain, env_release, env_hold;
    float amp;                /* 0..1 (+ slot gain), applied x2.63 at the filter stage */
    float pan;                /* -1..1 */
    int   portamento;
    float mode_switch_fade;   /* s */
    /* loop keys */
    int   loop_division;      /* 2..32 */
    int   playhead_return;    /* EC_RETURN_* (release of the last loop key) */
    /* E3: the other warp modes (params_spec units are already engine units) */
    float harm_interval[3];   /* harmonizer IN1..IN3, st; 0 = that voice off */
    float wsola_window;       /* random_ola grain length, s 0.005..0.5 */
    float wsola_search;       /* random_ola wander, s 0..0.1 */
    float pv_window;          /* pitch_corrected window, s 0.005..2 */
    float pv_dispersion;      /* pitch_corrected pitch dispersion 0..1 */
    int   wt_window;          /* wavetable slices 2..64 */
    float wt_cycle;           /* wavetable cycle width, samples 16..8192 */
    float wt_lfo_rate;        /* Hz 0..8000 */
    float wt_lfo_depth;       /* -1..1 (params_spec 0..128, 64 = off) */
    int   wt_lfo_shape;       /* option 1..5: sine tri saw s&h rand */
    float freeze_amount;      /* 0..1 (> 0 freezes) */
    float spectral_blur;      /* 0..1 */
    float formant_shift;      /* st -24..24 */
    /* E4: sends, the insert FX, the MOD sources */
    int   send_tap;               /* 1 pre insert, 2 post insert */
    float send1_level, send2_level;   /* 0..1, Lag 0.02 */
    ec_fx_params_t fx;            /* the track's insert (fx_insert1_machine + its params) */
    ec_fx_params_t fx2;           /* 0.6.1: the second insert, after the first (NONE: skipped, costs nothing) */
    ec_mod_params_t mod;          /* LFO 1/2, mod env, velocity, macros */
} ec_track_params_t;

/* Fills p with the params_spec.lua defaults (already xformed; env_range = 8 s). */
void ec_track_params_default(ec_track_params_t *p);

/* ---------------------------------------------------------------------------
 * Events: sample-accurate within the NEXT ec_dsp_process() call.
 * offset = frame 0..frames-1 of that block (clamped). Events at the same
 * offset run in the order they were queued.
 */
enum {
    /* global */
    EC_EV_TRANSPORT = 1,  /* value: 1 start / 0 stop. flags EC_TF_RESET on start:
                             every loop track's playhead to 0 of its region and the
                             step-lock / loop-key anchors cleared (enter_step(reset)).
                             Stop releases every slice voice and closes loop_trig gates. */
    EC_EV_TEMPO = 2,      /* value: BPM */
    EC_EV_PANIC = 3,      /* steal (20 ms fade) every slice voice ("stop twice") */

    /* per track (ev.track = 0..5) */
    EC_EV_TRIG = 10,      /* a step fires. Dispatch by machine:
         loop      : note-on (amp + filter env, gate = length; length <= 0 = held
                     until EC_EV_NOTE_OFF) unless EC_TF_GHOST; if the step locks the
                     region (start and/or end >= 0) that becomes the "seq" region
                     layer: EC_TF_JUMP -> playhead to the region start, else
                     within-or-warp; ev.mode = its trig_release mode (EC_RETURN_*)
                     used when EC_EV_REGION_RELEASE ends it.
         loop_trig : note-on + region (lock or base) from phase 0 + reader gate
                     open for `length` samples (then 10 ms fade, reader parks).
         slice machines : one slice voice, as EC_EV_SLICE_ON. */
    EC_EV_NOTE_ON = 11,   /* amp + filter env retrigger, gate = length (<=0: held) */
    EC_EV_NOTE_OFF = 12,  /* close a held note's gate (release stage) */
    EC_EV_REGION_RELEASE = 13, /* the step region lock ended (note length expired, or a
                             new mono step cleared it): the region falls back to keys /
                             base and the playhead follows the lock's release mode. */
    EC_EV_REGION = 14,    /* low level: set the BASE region now. start/end 0..128,
                             mode: EC_RM_* , phase for EC_RM_PHASE */
    EC_EV_SLICE_ON = 15,  /* slice = 1..32; start/end = slice range 0..128 (track
                             space) or < 0 for the grid division of slice_count;
                             velocity, pitch (st, absolute), length (gate samples,
                             0 = one-shot / natural length), choke 0..8; mode 1..6 =
                             a per-slice play mode (E3), 0 = the track's.
                             Reverse / sync / rate come from the params. */
    EC_EV_SLICE_OFF = 16, /* release slice `slice` (live key up) */
    EC_EV_LOOP_KEY = 17,  /* loop key `slice` = 1..32, value 1 press / 0 release.
                             EC_TF_AUDITION on a press while stopped: the reader
                             plays the key region until the last key is released. */
    EC_EV_RELEASE_ALL = 18, /* release every slice voice of the track (gate off) */
    EC_EV_MOD_TRIG = 19   /* E4 (\trModTrig): EC_TF_LFO retriggers the LFOs (TRIG / ONE /
                             HOLD modes), EC_TF_MENV the mod envelope with gate `length`
                             samples (<= 0: held until EC_EV_NOTE_OFF) */
};

/* event flags */
#define EC_TF_RESET    1
#define EC_TF_JUMP     2
#define EC_TF_GHOST    4
#define EC_TF_AUDITION 8
#define EC_TF_LFO      16   /* E4: EC_EV_MOD_TRIG */
#define EC_TF_MENV     32

/* region modes for EC_EV_REGION */
enum {
    EC_RM_KEEP = 0,      /* move the fences, the read position keeps going (softcut) */
    EC_RM_PHASE = 1,     /* jump to `phase` (0..1) of the new region (crossfaded) */
    EC_RM_WITHIN = 2,    /* keep the absolute position if inside, else jump to start */
    EC_RM_PRESERVE = 3   /* keep the absolute position, wrapped into the region */
};

typedef struct ec_event {
    int   offset;     /* frame within the next block */
    int   type;       /* EC_EV_* */
    int   track;      /* 0..EC_TRACKS-1 */
    int   flags;      /* EC_TF_* */
    int   slice;      /* slice index / loop key 1..32 */
    int   mode;       /* EC_RM_* or EC_RETURN_* depending on type */
    int   choke;      /* 0 none, 1..8 */
    int   length;     /* samples */
    float value;      /* transport 1/0, bpm, key press 1/0 */
    float start, end; /* 0..128 track space, < 0 = not set */
    float phase;      /* 0..1 */
    float velocity;   /* 0..1 */
    float pitch;      /* semitones */
} ec_event_t;

/* ---------------------------------------------------------------------------
 * Read-back for the UI (cheap: values are kept up to date per block).
 */
typedef struct ec_track_status {
    float pos_sample;       /* 0..1 playhead within the whole sample */
    float pos_region;       /* 0..1 playhead within the playing region */
    float region_start, region_end;  /* playing region, 0..1 of the sample */
    int   reader_on;        /* loop reader audible (playing or fading) */
    int   voices;           /* sounding slice voices (incl. releasing) */
    int   voice_slice[EC_MAX_VOICES_TRACK];  /* slice index of voice i */
    float voice_pos[EC_MAX_VOICES_TRACK];    /* 0..1 within the sample */
    uint32_t keys_held;     /* loop keys bitmask (bit k-1) */
    float peak_l, peak_r;   /* post-track peak, decaying */
    float filter_env;       /* filter env cutoff contribution, semitones */
    int   mode_implemented; /* 0 when the warp mode falls back (spectral budget: tape) */
    /* E4 */
    int   fx_running;       /* the insert machine audible now (EC_FX_*) */
    ec_fx_meter_t fx_meter; /* COMP / DUCK / LIMIT gain reduction */
    int   fx2_running;      /* 0.6.1: the second insert's */
    float mod[EC_MOD_NDEST];/* the live mod sums (pitch cutoff res amp pan, -1..1) */
    float macro[4];         /* the macros' effective values */
    int   idle;             /* paused by the idle CPU saver */
    float src[EC_MSRC_N];   /* 0.5.0: the mod sources (LFO1 LFO2 ENV VEL AT MW EXPR, each at its level), 0 when off */
} ec_track_status_t;

/* E4: the send buses and the master insert (bus 0 = S1, 1 = S2, 2 = master) */
typedef struct ec_bus_status {
    float peak_l, peak_r;   /* the return (S1/S2) or the master insert's output, decaying */
    int   fx_running;
    ec_fx_meter_t fx_meter;
} ec_bus_status_t;

typedef struct ec_dsp ec_dsp_t;

/* Bytes of memory the engine needs (one block, 16-byte aligned): the state
 * (E3: 6 x 512 KB post-stage delay lines + FFT state) plus 9 FX slots' delay
 * memory (6 inserts, S1, S2, master): 17 523 904 bytes = 16.7 MiB in 0.2.0;
 * 0.6.1: 15 slots (two inserts a track). EC_DSP_MEM_MAX bounds it for static
 * arenas; ec_dsp.c fails to compile when ec_dsp_mem_size() > EC_DSP_MEM_MAX. */
#define EC_DSP_MEM_MAX ((size_t)26u << 20)
size_t ec_dsp_mem_size(void);
/* Initialise in mem (ec_dsp_mem_size() bytes, 16-byte aligned) at rate fs.
 * Every track starts with ec_track_params_default(), no sample, 120 BPM,
 * transport stopped, master volume 1. Returns the engine handle (== mem). */
ec_dsp_t *ec_dsp_init(void *mem, float fs);

void ec_dsp_track_params(ec_dsp_t *dsp, int track, const ec_track_params_t *p);
const ec_track_params_t *ec_dsp_get_track_params(const ec_dsp_t *dsp, int track);
void ec_dsp_set_sample(ec_dsp_t *dsp, int track, const ec_sample_t *s);
void ec_dsp_set_master_volume(ec_dsp_t *dsp, float vol);   /* 0..1.5 (MASTER pot) */
void ec_dsp_set_tempo(ec_dsp_t *dsp, float bpm);            /* immediate */

/* Queue one event for the next block. Returns 0, or -1 when the queue is full. */
int  ec_dsp_event(ec_dsp_t *dsp, const ec_event_t *ev);

/* E6: the live input (interleaved L,R floats, nominal +-1) for the INPUT machine: the next ec_dsp_process calls read
 * it frame by frame from its start (frame i of it at the i-th frame they render), silence past `frames`; NULL = none.
 * The memory must stay valid until those calls are done. */
void ec_dsp_input(ec_dsp_t *dsp, const float *in_lr, int frames);

/* Render `frames` stereo frames (interleaved L,R floats, nominal +-1). */
void ec_dsp_process(ec_dsp_t *dsp, float *out_lr, int frames);

void  ec_dsp_track_status(const ec_dsp_t *dsp, int track, ec_track_status_t *st);
/* 0.5.0: the mod matrix's live controls on a track (aftertouch, mod wheel, expression; each 0..1, smoothed) and its
 * sources as of the last block (EC_MSRC_N values, the LFOs / ENV / VEL at their levels; 0 while no slot is live) */
void  ec_dsp_mod_live(ec_dsp_t *dsp, int track, float at, float mw, float expr);
void  ec_dsp_mod_sources(const ec_dsp_t *dsp, int track, float *src);
/* 0.6.0: the live grains (docs/UX-V2.md LOOP ENGINE "Live grains"). ec_dsp_grains fills out[] (room for
 * EC_GRAIN_VIEW_MAX) with the track's sounding grains as the last block left them and returns their count: the GRAIN
 * warp's cloud, the grain clouds of the track's slice voices in that warp, and random_ola's TGrains grains; 0 for
 * every other warp, for a track that is not reading (stopped, no sample) and for an idle track. pos = the grain's
 * read position now, len = the span of the sample it reads over its life (|rate| x duration), both 0..1 of the
 * whole sample; level = its window (Hann) now, 0..1; newest = 1 on the most recently spawned grain (exactly one when
 * n > 0). More than EC_GRAIN_VIEW_MAX grains: the newest plus the loudest others. Computed at the call from the
 * engine's grain state (nothing is added to the audio path); call it between blocks, once per screen frame.
 * The screen kit (src/ui/ec_ui_kit.h) carries the same guarded fallback of the struct. */
#ifndef EC_GRAIN_VIEW_MAX
typedef struct {
    float pos, len, level;   /* pos / len: 0..1 of the sample; level: the envelope 0..1 */
    uint8_t newest;
} ec_grain_view_t;
#define EC_GRAIN_VIEW_MAX 16
#endif
int   ec_dsp_grains(ec_dsp_t *dsp, int track, ec_grain_view_t *out);
/* E4: bus 0 S1, 1 S2, 2 master insert */
void  ec_dsp_bus_params(ec_dsp_t *dsp, int bus, const ec_fx_params_t *p);
void  ec_dsp_bus_status(const ec_dsp_t *dsp, int bus, ec_bus_status_t *st);
void  ec_dsp_master_peak(const ec_dsp_t *dsp, float *l, float *r);
int   ec_dsp_total_voices(const ec_dsp_t *dsp);
/* 0.6.1: the counted load (ec_load.c), smoothed over ~0.25 s: what runs, weighted by its measured wasm3 cost; 1.0 =
 * the hosts' budget (2x real time). Counted, never timed: it follows the audio state only. */
float ec_dsp_load(const ec_dsp_t *dsp);
int   ec_dsp_playing(const ec_dsp_t *dsp);
float ec_dsp_tempo(const ec_dsp_t *dsp);

/* ---------------------------------------------------------------------------
 * Value transforms (lib/elasticat.lua param_xforms), spec units -> engine.
 */
float ec_xf_amount(float v127);        /* 0..127 -> 0..1 */
float ec_xf_bipolar(float v128);       /* 0..128 -> -1..1 (64 = 0) */
float ec_xf_morph(float v128);         /* 0..128 -> 0..1 */
float ec_xf_cutoff_hz(float v127);     /* 0..127 -> 20..20000 Hz, exponential */
/* 0..127 -> seconds on the Elektron curve, 128 -> EC_ENV_INF_SECONDS.
 * env_range is the params_spec option index 1..10 (1s 4s 8s ... 1024s; default 3). */
float ec_xf_env_seconds(float v, int env_range);

/* Small event helpers (fill an ec_event_t and queue it). */
int ec_dsp_note_on(ec_dsp_t *dsp, int offset, int track, int length);
int ec_dsp_loop_key(ec_dsp_t *dsp, int offset, int track, int key, int pressed, int flags);
int ec_dsp_transport(ec_dsp_t *dsp, int offset, int start, int reset);

#ifdef __cplusplus
}
#endif
#endif /* EC_DSP_H */
