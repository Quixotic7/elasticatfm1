/*
 * ec_internal.h -- engine internals shared by the engine source files.
 * Not part of the public API.
 */
#ifndef EC_INTERNAL_H
#define EC_INTERNAL_H

#include <math.h>
#include <stdint.h>
#include <string.h>
#include "ec_dsp.h"
#include "ec_warp3.h"   /* E3 */

#define EC_KR          16      /* control period (samples): smoothers, filter coefs */
#define EC_HANN_SIZE   2048    /* shared Hann window LUT (+1 guard point) */
#define EC_MAX_GRAINS  72      /* ElasticatGrains kMaxGrains */
#define EC_VGRAINS     16      /* grains per slice voice (granular warp; SC Warp1) */
#define EC_VOICE_POOL  48      /* live voices + voices fading out after a steal */
#define EC_COMB_LEN    4096    /* comb delay line (>= 0.05 s at 48 kHz), power of 2 */
#define EC_PI          3.14159265358979323846
#define EC_HALF_PI_F   1.57079632679f

/* ---------------------------------------------------------------- helpers */

static inline float ec_clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline double ec_clampd(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
static inline int ec_clampi(int x, int lo, int hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/* SC cubicinterp: 4-point, 3rd-order Hermite (x-form). */
static inline float ec_hermite(float x, float y0, float y1, float y2, float y3)
{
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3;
    float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * x + c2) * x + c1) * x + y1;
}

/* sin(x * pi/2) for x in [0,1] (equal-power fade law), max error ~1e-4. */
static inline float ec_sin_hp(float x)
{
    /* odd polynomial in t = x*pi/2 (Taylor to t^7 is enough on [0,pi/2]) */
    float t = x * EC_HALF_PI_F, t2 = t * t;
    return t * (1.f - t2 * (1.f / 6.f - t2 * (1.f / 120.f - t2 * (1.f / 5040.f))));
}

/* tanh, rational approximation (exact at 0, |err| < 3e-3, clamps beyond +-3). */
static inline float ec_tanh(float x)
{
    if (x > 3.f) return 1.f;
    if (x < -3.f) return -1.f;
    float x2 = x * x;
    return x * (27.f + x2) / (27.f + 9.f * x2);
}

static inline float ec_midiratio(float st) { return exp2f(st * (1.f / 12.f)); }

static inline uint32_t ec_xorshift(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return x;
}
/* [-1,1) like the UGens' frand() */
static inline float ec_frand2(uint32_t *s)
{
    return ((float)(ec_xorshift(s) & 0xFFFFFFu) / (float)0x800000) - 1.f;
}

/* Interleaved int16 stereo frame access. */
static inline float ec_s16(int16_t v) { return (float)v; }
#define EC_S16_SCALE (1.f / 32768.f)
#define EC_HEADROOM 0.5f      /* 0.6.56 (owner): the sources at -6 dB into the chain (no internal clipping) */

/* Cubic read with indices clamped to [0, frames-1] (ElasticatReader peekBuf /
 * egPeek). Returns UNSCALED int16 units in *l, *r. */
static inline void ec_peek_clamp(const ec_sample_t *s, double pos, float *l, float *r)
{
    int fr = (int)s->frames;
    double mx = (double)(fr - 1);
    if (pos < 0.0) pos = 0.0;
    if (pos > mx) pos = mx;
    int i1 = (int)pos;
    float x = (float)(pos - (double)i1);
    int i0 = i1 - 1; if (i0 < 0) i0 = 0;
    int i2 = i1 + 1; if (i2 > fr - 1) i2 = fr - 1;
    int i3 = i1 + 2; if (i3 > fr - 1) i3 = fr - 1;
    const int16_t *d = s->data;
    *l = ec_hermite(x, d[i0 * 2], d[i1 * 2], d[i2 * 2], d[i3 * 2]);
    if (s->channels == 1) *r = *l;
    else *r = ec_hermite(x, d[i0 * 2 + 1], d[i1 * 2 + 1], d[i2 * 2 + 1], d[i3 * 2 + 1]);
}

/* Cubic read, CIRCULAR over [0,frames) (ElasticatGrains peekCubWrap, BufRd loop:1).
 * pos may be anywhere; it is wrapped. */
static inline void ec_peek_wrap(const ec_sample_t *s, double pos, float *l, float *r)
{
    int fr = (int)s->frames;
    double fb = (double)fr;
    if (pos >= fb || pos < 0.0) { pos = fmod(pos, fb); if (pos < 0.0) pos += fb; }
    int i1 = (int)pos;
    if (i1 >= fr) i1 = fr - 1;
    float x = (float)(pos - (double)i1);
    int i0 = i1 - 1; if (i0 < 0) i0 += fr;
    int i2 = i1 + 1; if (i2 >= fr) i2 -= fr;
    int i3 = i1 + 2; if (i3 >= fr) i3 -= fr;
    const int16_t *d = s->data;
    *l = ec_hermite(x, d[i0 * 2], d[i1 * 2], d[i2 * 2], d[i3 * 2]);
    if (s->channels == 1) *r = *l;
    else *r = ec_hermite(x, d[i0 * 2 + 1], d[i1 * 2 + 1], d[i2 * 2 + 1], d[i3 * 2 + 1]);
}

/* ---------------------------------------------------------- envelopes */
/* One SC EnvGen segment: hold, linear, curve (shape value) or sine. */
enum { SEG_HOLD = 0, SEG_LIN = 1, SEG_CURVE = 2, SEG_SIN = 3 };

typedef struct {
    double level, end;
    double a2, b1, grow;  /* curve: level = a2 - b1, b1 *= grow */
    double y1, y2;        /* sine recurrence */
    double slope;         /* linear */
    int32_t counter;      /* samples left; < 0 = infinite hold */
    int   shape;
} ec_seg_t;

void ec_seg_start(ec_seg_t *s, float end, float secs, int shape, float curve, float fs);

static inline float ec_seg_tick(ec_seg_t *s)
{
    float out = (float)s->level;
    if (s->counter > 0) {
        switch (s->shape) {
        case SEG_LIN: s->level += s->slope; break;
        case SEG_CURVE: s->b1 *= s->grow; s->level = s->a2 - s->b1; break;
        case SEG_SIN: {
            double y0 = s->b1 * s->y1 - s->y2;
            s->level = s->a2 - y0; s->y2 = s->y1; s->y1 = y0;
        } break;
        default: break;
        }
        if (--s->counter == 0) s->level = s->end;
    }
    return out;
}

/* Envelope stages */
enum { ENV_IDLE = 0, ENV_ATK, ENV_DEC, ENV_SUS, ENV_HOLD, ENV_REL, ENV_DONE };

typedef struct {
    int   mode;            /* EC_ENV_ADSR / EC_ENV_AHR / EC_ENV_GATED (voices) */
    float atk, dec, sus, rel, hold;
} ec_envspec_t;
#define EC_ENV_GATED 3     /* slice voice: ADSR-shaped with release node, frees at end */

typedef struct {
    ec_seg_t seg;
    int stage;
    int gate;              /* window open */
    int32_t gate_left;     /* samples until the window closes, -1 = indefinite */
} ec_env_t;

void  ec_env_reset(ec_env_t *e);
/* note-on. legato (portamento): no retrigger while the window is open. */
void  ec_env_trigger(ec_env_t *e, const ec_envspec_t *sp, int32_t gate_samples,
                     int legato, float fs);
void  ec_env_note_off(ec_env_t *e, const ec_envspec_t *sp, float fs);
void  ec_env_gate_off(ec_env_t *e, const ec_envspec_t *sp, float fs); /* any window */
void  ec_env_next(ec_env_t *e, const ec_envspec_t *sp, float fs);     /* segment ended */

static inline float ec_env_tick(ec_env_t *e, const ec_envspec_t *sp, float fs)
{
    float v = ec_seg_tick(&e->seg);
    if (e->seg.counter == 0) ec_env_next(e, sp, fs);
    if (e->gate_left > 0 && --e->gate_left == 0) ec_env_gate_off(e, sp, fs);
    return v;
}

/* ---------------------------------------------------------- reader */
/* ElasticatReader: softcut-model two-head reader with one queued crossfade. */
enum { RH_STOP = 0, RH_FADEIN = 1, RH_FADEOUT = 2, RH_PLAY = 3 };

typedef struct {
    double phase[2];
    float  fade[2];
    int    state[2];
    int    hactive[2];
    int    active;
    double queued_pos;
    int    queued;        /* 1 = jump to queued_pos, 2 = loop wrap (computed at cut) */
} ec_reader_t;

void ec_reader_init(ec_reader_t *r);
void ec_reader_cut(ec_reader_t *r, double pos);        /* cutToPos */
void ec_reader_process(ec_reader_t *r, const ec_sample_t *s, double lo, double hi,
                       double rate, double start_pos, float fade_inc,
                       float *L, float *R, int n);
static inline int ec_reader_idle(const ec_reader_t *r)
{
    return r->state[0] == RH_STOP && r->state[1] == RH_STOP;
}

/* ---------------------------------------------------------- grains */
typedef struct { double pos, rate; float phase, inc; } ec_grain_t;

typedef struct {
    ec_grain_t g[EC_MAX_GRAINS];
    int      n;
    double   spawn;
    uint32_t rng;
} ec_grains_t;

/* ElasticatGrains: emit at center[i] (frames), returns summed (unscaled) L/R. */
void ec_grains_process(ec_grains_t *gs, const ec_sample_t *s, const float *hann,
                       double emit0, double emit_inc, double density, double cycle_samples,
                       double spread_frames, double base_rate, double speed_rand,
                       double dir_balance, float *L, float *R, int n);

static inline float ec_hann_lerp(const float *hann, float ph)
{
    float x = ph * (float)EC_HANN_SIZE;
    int i = (int)x;
    float f = x - (float)i;
    return hann[i] + f * (hann[i + 1] - hann[i]);
}

/* ---------------------------------------------------------- chopped */
typedef struct {
    double pos, rate;
    ec_seg_t env;
    int stage;            /* 0 idle, 1 atk, 2 sus, 3 rel */
} ec_chop_head_t;

typedef struct {
    ec_chop_head_t h[2];
    int    active;
    double ramp;          /* 0..1 loop ramp */
    double imp;           /* step impulse phase 0..1 */
    int    primed;        /* first impulse fires at phase 0 */
} ec_chop_t;

/* ---------------------------------------------------------- slice voices */
typedef struct {
    int      used, track, slot, counted, choke, stealing;
    uint32_t serial;
    int      kind;        /* 0 raw read, 1 grain cloud (granular warp) */
    int      play_mode;   /* 0..5 */
    int      reverse;
    double   rate;        /* signed read rate, frames/sample */
    double   read_lo, read_hi, reset_frame, range, sweep, loop_pos;
    double   pos;         /* current read (for UI) */
    float    vel, steal_gain, steal_dec;
    int32_t  slot_left;   /* samples until the gate Routine fires (-1 none) */
    ec_envspec_t spec;
    ec_env_t env;
    /* grain cloud (Warp1-like) */
    ec_grain_t g[EC_VGRAINS];
    int      ng;
    double   gspawn;
    float    gsize, goverlap, grandom;
    double   grate;
    uint32_t rng;
    int      pitched;     /* E4: pitch mod applies (raw tape/chopped, grain clouds) */
    double   pmod;        /* E4: this chunk's pitch-mod ratio */
} ec_voice_t;

/* ---------------------------------------------------------- filter */
typedef struct {
    float b1, b2, a0;     /* SC RLPF/RHPF/BPF style coefficients */
} ec_sc2p_t;

typedef struct {
    /* smoothed controls */
    float cut_lag, rq_lag, morph_lag, pan_lag, amp_lag;
    float fc_a, fc_b, rq;           /* last coef inputs (for caching) */
    int   type_cached, mach_cached;
    ec_sc2p_t lp[2], hp[2], bp[2];  /* per channel A/B */
    float st[2][3][2];              /* [ch][LP,HP,BP][y1,y2] */
    int   cur_type, prev_type;      /* classic type crossfade */
    int   xf_left;
    /* ladder */
    float lb0, la1, lk;
    float lad[2][4];
    /* svf (TPT) */
    float sg[2], sk, sa1[2], sa2[2], sa3[2];
    float svf[2][2];
    /* comb */
    float comb[2][EC_COMB_LEN];
    int   comb_w;
    float comb_dt[2], comb_fb;
    float clp_a0, clp_b1, clp_b2;   /* SC LPF (damp) */
    float clp[2][2];
    float comb_y[2];
    /* formant */
    ec_sc2p_t fm[2][3];
    float fst[2][3][2];
    float famp2, famp3;
    /* filter env */
    float env_semis;
    /* output stage gains (linear-interpolated per chunk) */
    float gl, gr;
    float dc_x1[2], dc_y1[2];
    int   first;
} ec_filter_t;

/* ---------------------------------------------------------- E4: mod */
typedef struct {
    int      alive;          /* the SC mod synth exists (some depth != 0) */
    float    phase[2], sweep[2], held[2], rnd[2], rnd_ph[2];
    float    vel, vel_lag;   /* the latched last trigger velocity */
    float    macro_val[4];   /* the macros' effective values (UI) */
    float    live[3], live_lag[3];   /* 0.5.0: AT, MW, EXPR 0..1 (set by the app), smoothed */
    float    src[EC_MSRC_N]; /* 0.5.0: the sources at the last chunk (each at its level; the sequencer reads them) */
    ec_env_t env;
    uint32_t rng;
} ec_mod_t;

void ec_mod_init(ec_mod_t *m, uint32_t seed);
void ec_mod_trig(ec_mod_t *m, const ec_mod_params_t *p, int lfo, int env, int32_t gate, float fs);
void ec_mod_note_off(ec_mod_t *m, const ec_mod_params_t *p, float fs);
void ec_mod_tick(ec_mod_t *m, const ec_mod_params_t *p, float bpm, float fs, const float *lag01, int n,
                 float *out);

/* ---------------------------------------------------------- tracks */
typedef struct {
    int    valid;
    double start, end;    /* engine fractions 0..1 */
    double pos;           /* engine fraction */
    double time;          /* sample clock */
} ec_anchor_t;

enum { FAM_NONE = 0, FAM_READER = 1, FAM_GRAINS = 2, FAM_CHOP = 3,
       FAM_DOMINO = 4, FAM_WT = 5, FAM_SPEC = 6 /* E3 */ };

typedef struct {
    ec_track_params_t p;
    ec_sample_t smp;
    int    has_smp;

    /* derived (per block) */
    double frames;         /* sample frames */
    double brs;            /* BufRateScale: src rate / fs */
    double steps_whole;    /* sampleSteps scaled to the whole buffer, 1..512 */
    double src_bpm;        /* derivedSourceBpm */

    /* region layers (track space 0..128) */
    float  cur_s, cur_e;   /* applied region (resolved) */
    int    cur_valid;
    int    seq_has_s, seq_has_e;
    float  seq_s, seq_e;
    int    seq_release_mode;
    ec_anchor_t seq_anchor, keys_anchor;
    uint32_t keys;
    uint32_t keys_span;     /* 0.6.12: every key of this entry (a release never shrinks the region) */
    int    audition;
    double lo, hi;         /* applied region, engine fractions */

    /* source families */
    int    fam, prev_fam;
    float  mode_gain, prev_gain, mode_fade_inc;
    int    mode;           /* warp mode in effect */
    ec_reader_t rd;
    double restart_frame;  /* reader auto-start position */
    ec_grains_t gr;
    double emit_ph;
    ec_chop_t ch;
    int    lt_open;        /* loop_trig gate */
    int    live_gate;      /* 0.6.62 (owner): a live key (loop key / KEYBOARD) held while playing: TRIG sounds */
    int32_t lt_left;

    /* smoothed controls (kr) */
    float  play_gate;      /* Lag 0.01 */
    float  rate_lag;       /* warp_rate, Lag 0.02 */
    float  pitch_lag;      /* Lag 0.03 */
    float  gsize_lag, gspread_lag, gspeed_lag;   /* Lag 0.05 */
    float  mute_gain;      /* Lag 0.03 */
    float  slew_tab[EC_KR + 1];  /* filter_slew per-chunk coefficients */
    float  slew_cached;

    /* envelopes */
    ec_envspec_t amp_spec, fenv_spec;
    ec_env_t amp_env, fenv;

    ec_filter_t f;

    /* slice slots 1..32 -> voice index, -1 */
    int16_t slot_voice[33];

    /* declick tail after a sample swap */
    float  dk_l, dk_r, dk_step;
    int    dk_left;
    float  last_l, last_r;

    /* status */
    float  peak_l, peak_r;
    double ui_pos;         /* engine fraction */
    int    reader_on;

    /* E3: the other warp modes (ec_warp3.c) */
    int    w3_fallback;    /* the mode fell back (spectral budget) */
    ec_w3_t w3;
    /* E4: insert FX, sends, mod, the idle CPU saver */
    ec_fx_t fx;
    ec_fx_t fx2;                 /* 0.6.1: the second insert */
    ec_mod_t mod;
    float  mods[EC_MOD_NDEST];   /* this chunk's mod sums (pitch cutoff res amp pan) */
    float  snd1, snd2;           /* send levels, Lag 0.02 */
    float  mix_l, mix_r;         /* post-mix peak follower (idle detector, Amplitude 0.01/0.3) */
    int32_t quiet;               /* samples the track has been silent and stopped */
    int    idle;                 /* paused by the idle scan: the chain is skipped */
} ec_track_t;

typedef struct {
    int n;
    ec_event_t ev[EC_MAX_EVENTS];
} ec_evq_t;

struct ec_dsp {
    float  fs;
    float  bpm;
    int    playing;
    float  master_vol, master_lag;
    double clock;          /* sample counter */
    uint32_t serial;
    uint32_t rng;
    float  hann[EC_HANN_SIZE + 1];
    /* Lag coefficient tables: c^n for n = 0..EC_KR (per-sample c) */
    float  lag002[EC_KR + 1], lag01[EC_KR + 1], lag02[EC_KR + 1], lag03[EC_KR + 1],
           lag05[EC_KR + 1];
    ec_track_t tr[EC_TRACKS];
    ec_voice_t v[EC_VOICE_POOL];
    ec_evq_t q;
    float  peak_l, peak_r;
    /* scratch */
    float  bl[EC_KR], br[EC_KR], tl[EC_KR], tr2[EC_KR], ol[EC_KR], orr[EC_KR];
    float  ml[EC_KR], mr[EC_KR];
    /* E3: the neighbor tap (the left track's output for this chunk), spectral scratch */
    float  nbl[EC_KR], nbr[EC_KR];
    /* E6: the live input (ec_dsp_input): the caller's block, consumed as ec_dsp_process renders; in_at = the chunk's
     * first frame in it (track_chunk reads in + 2 * in_at) */
    const float *in;
    int    in_frames, in_at;
    ec_w3_shared_t w3s;
    /* E4: send buses S1 / S2 and the master insert (slots 0, 1, 2) */
    ec_fx_t bus[3];
    ec_fx_params_t busp[3];
    float  s1l[EC_KR], s1r[EC_KR], s2l[EC_KR], s2r[EC_KR], kl[EC_KR], kr[EC_KR];
    float  bus_peak[3][2];       /* S1 / S2 returns, the master insert's output */
    float  idle_grace;           /* s */
    float  load;                 /* 0.6.1: the counted load, smoothed (ec_load.c) */
    int    load_init;
};

/* 0.6.1: ec_load.c */
void  ec_load_update(ec_dsp_t *d, int frames);

/* lag tables */
void  ec_lag_table(float *tab, float lag_s, float fs);
static inline float ec_lag(float y, float x, const float *tab, int n)
{
    float d = y - x;
    d *= tab[n];
    if (d < 1e-9f && d > -1e-9f) return x;
    return x + d;
}

/* filter.c */
void ec_filter_reset(ec_filter_t *f);
void ec_filter_process(ec_dsp_t *dsp, ec_track_t *t, float *L, float *R, int n);

/* voices.c */
void ec_voice_slice_on(ec_dsp_t *dsp, int track, const ec_event_t *ev);
void ec_voice_release_slot(ec_dsp_t *dsp, int track, int slot);
void ec_voice_release_all(ec_dsp_t *dsp, int track);
void ec_voice_steal_all(ec_dsp_t *dsp, int track);   /* track < 0: every track */
void ec_voice_kill_track(ec_dsp_t *dsp, int track);  /* immediate (sample swap) */
void ec_voices_process(ec_dsp_t *dsp, int track, float *L, float *R, int n);

/* chop.c */
void ec_chop_reset(ec_chop_t *c, double phase);
void ec_chop_process(ec_dsp_t *dsp, ec_track_t *t, double cps, double pitch_rate,
                     double run, double dir, float *L, float *R, int n);

/* ec_dsp.c helpers shared with voices */
double ec_track_map(const ec_track_t *t, double x128);  /* track space -> 0..1 */

/* E3: ec_warp3.c -- the other warp modes */
void   ec_w3_init(ec_dsp_t *dsp);                        /* shared tables */
void   ec_w3_reset(ec_w3_t *w, uint32_t seed);
int    ec_w3_family(int mode);                           /* FAM_* of a warp mode */
int    ec_w3_stage(int mode);                            /* reader post stage (W3_ST_*) */
void   ec_w3_set_reader_mode(ec_w3_t *w, int mode, float fade_s, float fs);
/* reader rate (frames / sample, unsigned) of the reader-family mode in effect */
double ec_w3_reader_rate(const ec_dsp_t *d, const ec_track_t *t, double cps);
void   ec_w3_post(ec_dsp_t *d, ec_track_t *t, double cps, float *L, float *R, int n);
void   ec_w3_domino(ec_dsp_t *d, ec_track_t *t, double cps, double run, double dir, float *L, float *R, int n);
void   ec_w3_wavetable(ec_dsp_t *d, ec_track_t *t, float *L, float *R, int n);
void   ec_w3_spectral(ec_dsp_t *d, ec_track_t *t, double cps, double run, double dir, float *L, float *R, int n);
double ec_w3_pos(const ec_track_t *t, int fam);          /* playhead (engine fraction) of an E3 family */
void   ec_w3_fam_reset(ec_track_t *t, int fam, double phase);

#endif
