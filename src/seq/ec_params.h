/* SPDX-License-Identifier: GPL-3.0-only */
/* The per-track parameter model: a C port of Elasticat's lib/tracks/params_spec.lua (the subset the FM-1 engine
 * uses, plus the E4 params stored for later). Values are kept in SPEC units (what the Lua params held: 0..127
 * amounts, 0..128 bipolar / region points, option indices 1-based, seconds for the few time params), and turned into
 * the engine's ec_track_params_t by ec_params_engine() with the Lua xforms.
 *
 * The enum order is the project file's parameter key: NEVER reorder, only append (the save stores id/value pairs). */
#ifndef EC_PARAMS_H
#define EC_PARAMS_H
#include <stdint.h>

#include "ec_dsp.h"

enum {
    /* state + source */
    EC_P_MUTE, EC_P_MACHINE, EC_P_PITCH, EC_P_SLOT, EC_P_LOOP_START, EC_P_LOOP_END, EC_P_RANGE_START,
    EC_P_RANGE_END, EC_P_LOOP_REVERSE, EC_P_XFADE,
    /* warp */
    EC_P_MODE, EC_P_MODE_MACRO, EC_P_CHOP_STEPS, EC_P_CHOP_SLICE_LEN, EC_P_CHOP_LOOP_MODE, EC_P_CHOP_ATTACK,
    EC_P_CHOP_HOLD, EC_P_CHOP_RELEASE, EC_P_GRAIN_SIZE, EC_P_GRAIN_DENSITY, EC_P_GRAIN_JITTER, EC_P_GRAIN_SPEED,
    EC_P_GRAIN_SPEED_RAND, EC_P_GRAIN_DIRECTION, EC_P_WARP_RATE,
    /* slices */
    EC_P_SLICE_COUNT, EC_P_SLICE_INDEX, EC_P_SLICE_PLAY_MODE, EC_P_SLICE_REVERSE, EC_P_SLICE_RATCHET,
    EC_P_SLICE_SYNC, EC_P_SLICE_RATE, EC_P_SLICE_HOLD_TO_STEP, EC_P_SLICE_HOLD,
    /* trig (sequencer domain) */
    EC_P_PATTERN_STEPS, EC_P_TRIG_JUMP, EC_P_TRIG_RELEASE, EC_P_TRIG_CHANCE, EC_P_TRIG_CONDITION,
    EC_P_TRIG_RATCHET, EC_P_ENV_RESET, EC_P_LFO_RESET, EC_P_FILTER_RESET,
    /* filter */
    EC_P_FILTER_MACHINE, EC_P_FILTER_TYPE, EC_P_FILTER_CUTOFF, EC_P_FILTER_RES, EC_P_FILTER_DRIVE,
    EC_P_FILTER_MORPH, EC_P_FILTER_BALANCE, EC_P_FILTER_MIX, EC_P_FILTER_MIX_MODE,
    EC_P_FENV_MODE, EC_P_FENV_ATTACK, EC_P_FENV_DECAY, EC_P_FENV_SUSTAIN, EC_P_FENV_RELEASE, EC_P_FENV_HOLD,
    EC_P_FENV_DEPTH,
    /* amp */
    EC_P_ENV_MODE, EC_P_ENV_ATTACK, EC_P_ENV_DECAY, EC_P_ENV_SUSTAIN, EC_P_ENV_RELEASE, EC_P_ENV_HOLD,
    EC_P_ENV_RANGE,
    /* fx + sends (E4: stored) */
    EC_P_FX_MACHINE, EC_P_FX_DRIVE, EC_P_FX_MIX, EC_P_SEND_TAP, EC_P_SEND1, EC_P_SEND2,
    /* output */
    EC_P_AMP, EC_P_PAN,
    /* mod (E4: stored) */
    EC_P_LFO1_DEST, EC_P_LFO1_WAVE, EC_P_LFO1_SPEED, EC_P_LFO1_DEPTH, EC_P_LFO1_MODE,
    EC_P_LFO2_DEST, EC_P_LFO2_WAVE, EC_P_LFO2_SPEED, EC_P_LFO2_DEPTH, EC_P_LFO2_MODE,
    /* behaviour */
    EC_P_PORTAMENTO, EC_P_MODE_SWITCH_FADE, EC_P_LOOP_DIVISION, EC_P_TRIG_POLY,
    EC_P_LENGTH,          /* default_length (steps); its step lock is the trig's length */
    EC_P_VELOCITY,        /* default_velocity 0..1; its step lock is the trig's velocity */
    EC_P_RATE,            /* the track's pattern rate index 1..21 (TrackSequencer.RATE_FRACTIONS; 13 = 1x) */
    EC_NP
};

/* kinds */
enum { EC_PK_NUM, EC_PK_OPT, EC_PK_BIN };
/* display formats */
enum {
    EC_PF_INT, EC_PF_DEC1, EC_PF_DEC2, EC_PF_MS, EC_PF_ENV, EC_PF_HZ, EC_PF_BIP, EC_PF_PAN, EC_PF_PCT01,
    EC_PF_PCT100, EC_PF_X, EC_PF_PITCH, EC_PF_OPT, EC_PF_BIN, EC_PF_RATE, EC_PF_STEPS, EC_PF_SEC
};
/* flags */
#define EC_PF_LOCKABLE 1u      /* p-lockable per step */
#define EC_PF_REGION 2u        /* region / range point: a resolver input, never a held param (REGION_LOCKS) */
#define EC_PF_SEQ 4u           /* steers the sequencer, never the engine (SEQUENCER_DOMAIN_LOCKS) */
#define EC_PF_MOMENT 8u        /* consumed at trigger time, never held (length, velocity) */
#define EC_PF_E4 16u           /* stored only: its engine half lands in E4 */

typedef struct {
    const char *name;          /* params_spec suffix */
    const char *label;         /* the cell's short label (<= 5 chars) */
    float min, max, step;      /* step: one knob detent */
    float def;
    uint8_t kind, fmt;
    uint8_t flags;
    uint8_t nopt;              /* option count (EC_PK_OPT) */
    const char *const *opts;   /* option labels (EC_PK_OPT) */
} ec_param_def_t;

extern const ec_param_def_t EC_PARAMS[EC_NP];

/* the rate table (TrackSequencer.RATE_FRACTIONS): beats per step = 0.25 * den / num */
#define EC_RATES 21
#define EC_RATE_UNITY 13
extern const uint8_t EC_RATE_FRAC[EC_RATES][2];
const char *ec_rate_label(int index);      /* 1..21 -> "1/16" .. "2" */

/* the 17 trig conditions (lib/sequencer/trig_conditions.lua) */
enum { EC_TC_NONE, EC_TC_AB, EC_TC_FILL, EC_TC_NFILL, EC_TC_PRE, EC_TC_NPRE, EC_TC_NEI, EC_TC_NNEI, EC_TC_FIRST };
typedef struct { const char *label; uint8_t kind, a, b; } ec_trigcond_t;
#define EC_TRIGCONDS 17
extern const ec_trigcond_t EC_TRIGCOND[EC_TRIGCONDS];

/* names for the screens */
const char *ec_machine_name(int machine);   /* 1..8: "LOOP" .. */
const char *ec_warp_name(int mode);         /* 1..13: "TAPE" .. */
const char *ec_filter_name(int machine);    /* 1..6 */

void ec_params_default(float *v);           /* every param to its SPEC default (v[EC_NP]) */
float ec_param_clamp(int id, float x);      /* to the range (options / binaries rounded) */
/* one knob turn of `det` detents on param id from x (options step by one, numbers by step) */
float ec_param_nudge(int id, float x, int det);
/* the value as the cell shows it (<= 7 chars), env times need the track's env range (1..10) */
void ec_param_format(int id, float x, int env_range, char *out, int cap);
/* 0..1 position of x in the range, for bars */
float ec_param_frac(int id, float x);

/* The sample slot's metadata the engine needs (the slot pool, ec_project.h) */
typedef struct {
    float trim_start, trim_end;  /* 0..1 of the sample */
    float steps;                 /* steps the trim spans */
    float gain;                  /* the slot gain (x) */
} ec_slotmeta_t;

/* SPEC values -> engine params (the Lua xforms). playhead_return: the global option 1..3 */
void ec_params_engine(const float *v, const ec_slotmeta_t *slot, int playhead_return, ec_track_params_t *out);

#endif
