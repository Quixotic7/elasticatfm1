/* SPDX-License-Identifier: GPL-3.0-only */
/* The project: global settings, the sample slot pool and 16 patterns (lib/sequencer/pattern_store.lua). As in the
 * original, a pattern is a near-total snapshot: every track's parameters (machine, warp, filter, ...) and its steps
 * (trigs, slices, p-locks); the sample pool, BPM and the other globals live outside the patterns. Pure data, fixed
 * memory, no platform: the sequencer (ec_seq.h) plays it, the app edits it, ec_project_save/load turn it into the
 * versioned record kept in the flash image. */
#ifndef EC_PROJECT_H
#define EC_PROJECT_H
#include <stdint.h>

#include "ec_params.h"
#include "ec_razor.h"   /* E3 */

#define EC_NTRACK 6
#define EC_NPAT 64                /* 0.6.0: 4 banks x 16 (pattern index = bank * 16 + n) */
#define EC_MAXSTEPS 256           /* pattern_steps 1..256 = 16 pages of 16 */
#define EC_STEP_LOCKS 16          /* p-locks a step holds */
#define EC_NSLOT 128              /* the sample pool (0.6.0: 16 -> 128) */
#define EC_PATH 192

/* one step record (lib/sequencer/step.lua): the trig, the slices it fires, its p-locks. The trig's pitch, length and
 * velocity are p-locks of EC_P_PITCH / EC_P_LENGTH / EC_P_VELOCITY; a ghost trig is derived (resets all off). */
typedef struct {
    uint8_t trig;
    uint8_t nlock;
    uint8_t lock_id[EC_STEP_LOCKS];
    uint32_t slices;              /* bit s-1: slice s (slice machines) */
    float lock_val[EC_STEP_LOCKS];
} ec_step_t;

typedef struct {
    float v[EC_NP];               /* the track's params (SPEC units) */
    ec_step_t steps[EC_MAXSTEPS];
} ec_ptrack_t;

typedef struct {
    uint8_t used;                 /* 0: never populated (a load copies the live params, empty steps) */
    ec_ptrack_t tr[EC_NTRACK];
} ec_pattern_t;

/* a pool slot (the persisted part; the loaded handle lives in the app) */
typedef struct {
    char path[EC_PATH];           /* "/audio/..", "" empty */
    float bpm;                    /* the sample's BPM (file name rule, or set on the FILE page) */
    float steps;                  /* steps the trim spans (sample_steps) */
    float trim_start, trim_end;   /* 0..1 of the sample */
    float gain;                   /* x (0..4) */
} ec_slot_t;

/* pattern change quantization (PatternStore.QUANT) */
enum { EC_Q_SEQUENTIAL = 1, EC_Q_DIRECT_JUMP, EC_Q_DIRECT_START, EC_Q_TEMP_JUMP };

/* E5: the A/B scenes (lib/scene_store.lua), per-pattern extras, the slice programs (track_sequencer.lua's
 * slice_locks / slice_choke / slice_mute) -- types only; the code is in ec_scene.c and ec_e5seq.c (ec_e5.h) */
#define EC_NAME 17                /* pattern / project names: 16 characters + NUL */
#define EC_SLICE_LOCKS 8          /* p-locks one slice pad holds */
typedef struct {
    uint32_t mute;                /* bit s-1: slice s muted (FN + pad) */
    uint8_t choke[32];            /* choke group 0 none, 1..8 */
    uint8_t nlock[32];
    uint8_t lock_id[32][EC_SLICE_LOCKS];
    float lock_val[32][EC_SLICE_LOCKS];
} ec_slicemap_t;

typedef struct {
    char name[EC_NAME];           /* "" = unnamed */
    float bpm;                    /* the pattern's BPM (used unless the project's global BPM is on) */
    uint16_t page_loop[EC_NTRACK];/* bit p: page p+1 loops; 0 = every page (PAGE > LOOP) */
    ec_slicemap_t slices[EC_NTRACK];
} ec_pat_e5_t;

/* scene rows: the 6 tracks, then the E4 buses S1, S2, MST (the original morphs the send / master FX params too: "a
 * genuinely global continuous param (the master/send FX busses) contributes a single id") */
#define EC_SCN_ROWS (EC_NTRACK + 3)
typedef struct {
    float pos;                            /* the crossfader 0 = A .. 1 = B */
    uint8_t has[2][EC_SCN_ROWS][EC_NP];   /* scene s holds a value for (row, param) */
    float val[2][EC_SCN_ROWS][EC_NP];
    /* runtime (rebuilt by ec_scene_apply, not saved): the published morph = the resolver's source 2 */
    uint8_t on[EC_SCN_ROWS][EC_NP];
    float xf[EC_SCN_ROWS][EC_NP];
} ec_scenes_t;

typedef struct {
    char name[EC_NAME];           /* the project's name */
    int gbpm;                     /* 1: one BPM for every pattern (the original's "global bpm" pref) */
    ec_scenes_t sc;
} ec_proj_e5_t;

/* 0.6.0: live recording's settings, per project (docs/UX-V2.md 4), saved in the record's "ECRS" chunk */
enum { EC_REC_CAPTURE, EC_REC_FIXED, EC_REC_OVERDUB };       /* MODE */
enum { EC_METRO_OFF, EC_METRO_REC, EC_METRO_ALWAYS };        /* METRONOME: off / while recording / always */
typedef struct {
    uint8_t mode;                 /* EC_REC_* (default CAPTURE) */
    uint8_t bars;                 /* LENGTH for FIXED: 1 / 2 / 4 / 8 bars (default 1) */
    uint8_t countin;              /* COUNT-IN: 0 / 1 / 2 bars (default 0) */
    uint8_t metro;                /* EC_METRO_* (default REC) */
    float metro_level;            /* the click's own level 0..1 (default 0.5) */
    uint8_t automation;           /* AUTOMATION: 1 = knob moves are recorded as locks (ghost locks; default on) */
    uint8_t hard;                 /* QUANTIZE: 0 = the offset kept as an EC_P_MICRO lock (default), 1 = hard */
} ec_recset_t;
void ec_recset_default(ec_recset_t *r);

/* 0.6.0: PATTERN LAUNCH's banks */
#define EC_NBANK 4
#define EC_BANK_PATS 16
static inline int ec_pat_index(int bank, int n) { return bank * EC_BANK_PATS + n; }

typedef struct ec_project {
    float bpm;                    /* 20..300 */
    float swing;                  /* 50..75 (%) */
    int glen;                     /* global pattern length 1..256 (sequential switches engage on its boundary) */
    int quant;                    /* EC_Q_* */
    int rtn;                      /* playhead return after the last loop key: 1 return, 2 boomerang, 3 reset */
    int current;                  /* the live pattern 0..15 */
    ec_slot_t slot[EC_NSLOT];
    ec_pattern_t pat[EC_NPAT];
    ec_razor_t razor[EC_NTRACK];  /* E3: each track's razor slices + per-slice settings (chunk "RZR1") */
    /* E4: the send buses S1 / S2 and the master insert (global, as the original's send1_* / send2_* / master_*
     * params): their FX machine + params, in the same SPEC units and ids as a track's (only the EC_PF_FX ids and
     * fx_insert1_machine / fx_mix / fx_drive are used). Saved as the record's "EC4B" chunk. */
    float bus[3][EC_NP];
    /* E5 */
    ec_pat_e5_t pe5[EC_NPAT];
    ec_proj_e5_t e5;
    /* 0.6.0 */
    ec_recset_t rec;              /* live recording (chunk "ECRS") */
} ec_project_t;

void ec_project_init(ec_project_t *p);                       /* a new project */
static inline ec_ptrack_t *ec_project_track(ec_project_t *p, int t) { return &p->pat[p->current].tr[t]; }
void ec_slot_clear(ec_slot_t *s);
void ec_bus_default(float *v);           /* E4: a bus record's defaults (NONE) */

/* steps */
int ec_step_has_content(const ec_step_t *s);
int ec_step_lock_get(const ec_step_t *s, int id, float *val);   /* 1 if locked */
int ec_step_lock_set(ec_step_t *s, int id, float val);          /* 0 ok, -1 full */
int ec_step_lock_clear(ec_step_t *s, int id);                   /* 1 if it was locked */
void ec_step_clear(ec_step_t *s);
int ec_step_reset_active(const ec_step_t *s, const float *v, int reset_id);
int ec_step_is_ghost(const ec_step_t *s, const float *v);
int ec_step_locks_region(const ec_step_t *s);                   /* loop_start or loop_end locked */
/* quick tap (GridSequencer:toggle_step): a step with content is cleared, an empty one gets a trig (slice machines:
 * the track's slice index). Returns 1 when the step now has a trig */
int ec_step_toggle(ec_ptrack_t *tr, int index);
/* FN + tap (toggle_ghost_step): normal <-> ghost (the three resets locked off; region locks dropped) */
void ec_step_toggle_ghost(ec_ptrack_t *tr, int index);
/* hold a step + a slice key (toggle_step_slice): mono slice machines hold one slice, poly ones stack */
void ec_step_toggle_slice(ec_ptrack_t *tr, int index, int slice);
int ec_track_has_trigs(const ec_ptrack_t *tr);

/* patterns */
/* make pattern idx usable: a never-populated slot copies the live pattern's params with empty steps */
void ec_pattern_prepare(ec_project_t *p, int idx);
void ec_pattern_clear_steps(ec_pattern_t *pt);

/* the record: returns the bytes written (<= cap), or -1 when it does not fit (the app shows FULL; buf's bytes past
 * the old record may be overwritten, so a caller that must keep the old record saves into a scratch buffer first).
 * Version 2 (0.6.0) is sparse: a track's params are the ids that differ from the defaults, steps only when they have
 * content, values that are small integers in one byte. */
int32_t ec_project_save(const ec_project_t *p, uint8_t *buf, uint32_t cap);
/* the record as version 1 (0.1 .. 0.5: every param, f32 values): tests that build old records; 64-pattern /
 * 128-slot projects are written whole (an old firmware reads the first 16 of each) */
int32_t ec_project_save_v1(const ec_project_t *p, uint8_t *buf, uint32_t cap);
/* 0 ok (p replaced), -1 no project / bad / unknown version (p untouched, except on a damaged record of a known
 * version: then p is a fresh project). Versions 1 and 2; a version 1 record's 16 patterns land in bank A (0..15) and
 * its 16 slots in slots 1..16 */
int ec_project_load(ec_project_t *p, const uint8_t *buf, uint32_t n);
/* 0.5.0: the mod routes the last load dropped converting a 0.4 project (past a track's 8 matrix slots) */
int ec_project_mtx_dropped(void);

#define EC_PROJECT_MAGIC 0x4A504345u    /* "ECPJ" */
#define EC_PROJECT_VERSION 2          /* 0.6.0: sparse params, 64 patterns, 128 slots, "ECRS" */
#define EC_CHUNK_REC 0x53524345u      /* "ECRS": the record settings */

#endif
