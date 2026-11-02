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

#define EC_NTRACK 6
#define EC_NPAT 16
#define EC_MAXSTEPS 256           /* pattern_steps 1..256 = 16 pages of 16 */
#define EC_STEP_LOCKS 16          /* p-locks a step holds */
#define EC_NSLOT 16               /* the sample pool */
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

typedef struct {
    float pos;                            /* the crossfader 0 = A .. 1 = B */
    uint8_t has[2][EC_NTRACK][EC_NP];     /* scene s holds a value for (track, param) */
    float val[2][EC_NTRACK][EC_NP];
    /* runtime (rebuilt by ec_scene_apply, not saved): the published morph = the resolver's source 2 */
    uint8_t on[EC_NTRACK][EC_NP];
    float xf[EC_NTRACK][EC_NP];
} ec_scenes_t;

typedef struct {
    char name[EC_NAME];           /* the project's name */
    int gbpm;                     /* 1: one BPM for every pattern (the original's "global bpm" pref) */
    ec_scenes_t sc;
} ec_proj_e5_t;

typedef struct {
    float bpm;                    /* 20..300 */
    float swing;                  /* 50..75 (%) */
    int glen;                     /* global pattern length 1..256 (sequential switches engage on its boundary) */
    int quant;                    /* EC_Q_* */
    int rtn;                      /* playhead return after the last loop key: 1 return, 2 boomerang, 3 reset */
    int current;                  /* the live pattern 0..15 */
    ec_slot_t slot[EC_NSLOT];
    ec_pattern_t pat[EC_NPAT];
    /* E5 */
    ec_pat_e5_t pe5[EC_NPAT];
    ec_proj_e5_t e5;
} ec_project_t;

void ec_project_init(ec_project_t *p);                       /* a new project */
static inline ec_ptrack_t *ec_project_track(ec_project_t *p, int t) { return &p->pat[p->current].tr[t]; }
void ec_slot_clear(ec_slot_t *s);

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

/* the record: returns the bytes written (<= cap), or -1 when it does not fit */
int32_t ec_project_save(const ec_project_t *p, uint8_t *buf, uint32_t cap);
/* 0 ok (p replaced), -1 no project / bad / unknown version (p untouched) */
int ec_project_load(ec_project_t *p, const uint8_t *buf, uint32_t n);

#define EC_PROJECT_MAGIC 0x4A504345u    /* "ECPJ" */
#define EC_PROJECT_VERSION 1

#endif
