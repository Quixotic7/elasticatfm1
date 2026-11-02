/*
 * ec_fx.h -- E4: the FX machines (C port of the 18 \elasticatFx* SynthDefs in
 * Engine_Elasticat.sc, lib/fx_modes/registry.lua order).
 *
 * One ec_fx_t is one FX SLOT: a track's insert, send bus S1 / S2 return, or
 * the master insert. The slot runs one machine at a time; changing
 * p->machine swaps it click-free (the old machine fades to the slot's dry
 * signal -- silence on a send -- the new state is cleared, then fades in).
 *
 * Memory: the slot's delay lines live in caller-owned memory (EC_FX_MEM_FLOATS
 * floats, handed to ec_fx_init); every machine carves what it needs from it.
 * Pure C99, no allocation, no platform calls.
 *
 * Units: ec_fx_params_t holds ENGINE units = what the Lua sent to the
 * SynthDefs after its xform (amount 0..1, bipolar -1..1, beats, bits, Hz,
 * 0-based option indices for the SC Select args). ec_fx_params_default()
 * gives the params_spec.lua defaults, xformed.
 */
#ifndef EC_FX_H
#define EC_FX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* machine = fx_insert1_machine option index (registry.lua; engine index + 1) */
enum {
    EC_FX_NONE = 1, EC_FX_DELAY, EC_FX_REVERB, EC_FX_LOFI, EC_FX_COMP, EC_FX_DESTROY,
    EC_FX_ECHO, EC_FX_BLACKHOLE, EC_FX_CHORUS, EC_FX_FLANGER, EC_FX_PHASER, EC_FX_DJ_EQ,
    EC_FX_DUCK, EC_FX_TAPE_ECHO, EC_FX_CASSETTE, EC_FX_MOTION, EC_FX_RINGS, EC_FX_LIMIT,
    EC_FX_COUNT = 18
};

/* delay lines: 4 s (maxDelayTime) at up to 48 kHz, two channels */
#define EC_FX_MAX_DELAY_S   4.0f
#define EC_FX_LINE_FRAMES   (192000u + 1024u)
#define EC_FX_MEM_FLOATS    (2u * EC_FX_LINE_FRAMES)
#define EC_FX_MAX_N         64      /* frames per ec_fx_process call */

typedef struct ec_fx_params {
    int   machine;                       /* EC_FX_* */
    float mix;                           /* fx_mix 0..1 (fxMixBlend XFade2) */
    float drive;                         /* fx_drive 0..1 (DESTROY's DRIV) */
    /* delay */
    float delay_beats, delay_feedback, delay_tone;
    /* reverb (+ blackhole SIZE / HI) */
    float reverb_size, reverb_damp;
    /* lofi */
    float lofi_bits;                     /* 1..24 */
    float lofi_rate;                     /* Hz 1000..48000 */
    /* comp */
    float comp_thresh, comp_ratio, comp_attack, comp_release, comp_makeup;
    /* destroy */
    float destroy_warble, destroy_fold, destroy_sat, destroy_crush, destroy_srr,
          destroy_tone, destroy_level;
    /* echo (+ tape echo TIME / TONE) */
    float echo_beats, echo_offset;       /* offset -1..1 */
    int   echo_mode;                     /* 0 stereo, 1 ping-pong */
    float echo_tone, echo_wobble;
    /* blackhole */
    float bh_gravity, bh_mod, bh_low, bh_predelay;
    /* chorus / flanger / phaser (+ rings TONE) */
    float modfx_rate, modfx_depth, modfx_tone, chorus_width;
    float flanger_feedback;              /* -1..1 */
    float phaser_center;
    int   phaser_stages;                 /* 0..7 = 1..8 stages */
    float phaser_feedback;
    /* dj eq */
    float eq_low, eq_mid, eq_high;       /* -1..1 (0 unity, -1 kill) */
    float eq_xlow, eq_xhi;
    /* duck */
    float duck_amount, duck_sens, duck_attack, duck_release;
    /* tape echo / cassette */
    float tape_wow, tape_flutter, tape_sat;
    float cass_noise, cass_crackle, cass_drop, cass_tone;
    /* motion */
    float motion_width;                  /* -1..1 */
    float motion_beats;
    float motion_trem, motion_pan;
    int   motion_shape;                  /* 0 sine, 1 tri, 2 square */
    /* rings */
    int   rings_mode;                    /* 0 ring, 1 shift */
    float rings_freq, rings_fine;        /* -1..1 */
    float rings_feedback;
    /* limit */
    float limit_gain, limit_ceil, limit_release;
} ec_fx_params_t;

void ec_fx_params_default(ec_fx_params_t *p);
const char *ec_fx_name(int machine);     /* "DELAY", "TAPE ECHO", ... ("?" out of range) */

/* Dynamics meter of the running machine (COMP / DUCK / LIMIT; the SC
 * /elasticat/fxMeterRaw feed): gain reduction dB (<= 0), key / input level dB,
 * threshold dB (-99 = none, DUCK). valid = 0 for the other machines. */
typedef struct ec_fx_meter {
    float gr_db, key_db, thresh_db;
    int   valid;
} ec_fx_meter_t;

/* ---- the slot state (owned by ec_fx.c; sized here so the engine embeds it) ---- */
#include "ec_fx_state.h"

/* mem: EC_FX_MEM_FLOATS floats, 16-byte aligned, owned by the caller.
 * is_send: 1 for S1 / S2 returns (machine NONE = silence; the swap fades
 * through silence), 0 for inserts and the master (NONE = passthrough). */
void ec_fx_init(ec_fx_t *fx, float *mem, float fs, int is_send, uint32_t seed);

/* Process n (<= EC_FX_MAX_N) frames in place. bpm: the transport tempo (synced
 * times). keyL/keyR: the DUCK sidechain (the dry program), may be NULL. */
void ec_fx_process(ec_fx_t *fx, const ec_fx_params_t *p, float bpm,
                   const float *keyL, const float *keyR, float *L, float *R, int n);

void ec_fx_clear(ec_fx_t *fx);           /* drop every tail now (panic / double stop) */
int  ec_fx_running(const ec_fx_t *fx);   /* the machine audible now (EC_FX_*) */
void ec_fx_meter(const ec_fx_t *fx, ec_fx_meter_t *m);

#ifdef __cplusplus
}
#endif
#endif /* EC_FX_H */
